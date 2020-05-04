#include "Dialect/Stencil/StencilOps.h"
#include "Dialect/Stencil/StencilDialect.h"
#include "Dialect/Stencil/StencilTypes.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/BlockAndValueMapping.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Function.h"
#include "mlir/IR/Module.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/raw_ostream.h"
#include <bits/stdint-intn.h>
#include <cstddef>
#include <functional>
#include <iterator>

using namespace mlir;

//===----------------------------------------------------------------------===//
// stencil.apply
//===----------------------------------------------------------------------===//

static ParseResult parseApplyOp(OpAsmParser &parser, OperationState &state) {
  SmallVector<OpAsmParser::OperandType, 8> operands;
  SmallVector<OpAsmParser::OperandType, 8> arguments;
  ArrayAttr lbAttr, ubAttr;

  // Parse region arguments and the assigned data operands
  llvm::SMLoc loc = parser.getCurrentLocation();
  do {
    OpAsmParser::OperandType currentArgument;
    OpAsmParser::OperandType currentOperand;
    if (parser.parseRegionArgument(currentArgument) || parser.parseEqual() ||
        parser.parseOperand(currentOperand))
      return failure();
    arguments.push_back(currentArgument);
    operands.push_back(currentOperand);
  } while (!parser.parseOptionalComma());

  // Parse optional attributes and the operand types
  SmallVector<Type, 8> operandTypes;
  if (parser.parseColonTypeList(operandTypes))
    return failure();

  if (operands.size() != operandTypes.size()) {
    parser.emitError(parser.getCurrentLocation(), "expected ")
        << operands.size() << " operand types";
    return failure();
  }

  // Parse the body region.
  SmallVector<Type, 8> resultTypes;
  Region *body = state.addRegion();
  if (parser.parseRegion(*body, arguments, operandTypes))
    return failure();

  // Parse the optional bounds
  if (!parser.parseOptionalKeyword("to")) {
    // Parse the optional bounds
    if (parser.parseLParen() ||
        parser.parseAttribute(lbAttr, stencil::LoadOp::getLBAttrName(),
                              state.attributes) ||
        parser.parseColon() ||
        parser.parseAttribute(ubAttr, stencil::LoadOp::getUBAttrName(),
                              state.attributes) ||
        parser.parseRParen())
      return failure();

    // Make sure bounds have the right number of dimensions
    if (lbAttr.size() != stencil::kNumOfDimensions ||
        ubAttr.size() != stencil::kNumOfDimensions) {
      parser.emitError(
          parser.getCurrentLocation(),
          "expected bounds to have a component for every dimension");
      return failure();
    }
  }

  // Parse the return types and resolve all operands
  if (parser.parseOptionalAttrDict(state.attributes) ||
      parser.parseColonTypeList(resultTypes) ||
      parser.resolveOperands(operands, operandTypes, loc, state.operands) ||
      parser.addTypesToList(resultTypes, state.types))
    return failure();

  return success();
}

static void print(stencil::ApplyOp applyOp, OpAsmPrinter &printer) {
  printer << stencil::ApplyOp::getOperationName() << ' ';

  // Print the region arguments
  ValueRange operands = applyOp.getOperands();
  if (!applyOp.region().empty() && !operands.empty()) {
    Block *body = applyOp.getBody();
    llvm::interleaveComma(
        llvm::seq<int>(0, operands.size()), printer, [&](int i) {
          printer << body->getArgument(i) << " = " << operands[i];
        });
  }

  // Print the operand types
  printer << " : ";
  llvm::interleaveComma(applyOp.getOperandTypes(), printer);

  // Print region, bounds, and return type
  printer.printRegion(applyOp.region(),
                      /*printEntryBlockArgs=*/false);
  if (applyOp.lb().hasValue() && applyOp.ub().hasValue()) {
    printer << " to (";
    printer.printAttribute(applyOp.lb().getValue());
    printer << ":";
    printer.printAttribute(applyOp.ub().getValue());
    printer << ")";
  }
  printer.printOptionalAttrDict(
      applyOp.getAttrs(), /*elidedAttrs=*/{stencil::ApplyOp::getLBAttrName(),
                                           stencil::ApplyOp::getUBAttrName()});
  printer << " : ";
  llvm::interleaveComma(applyOp.res().getTypes(), printer);
}

namespace {
/// This is a pattern to remove duplicate results
struct ApplyOpResCleaner : public OpRewritePattern<stencil::ApplyOp> {
  using OpRewritePattern<stencil::ApplyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(stencil::ApplyOp applyOp,
                                PatternRewriter &rewriter) const override {
    // Get the terminator and compute the result list
    auto returnOp = cast<stencil::ReturnOp>(applyOp.getBody()->getTerminator());
    unsigned unrollFactor = returnOp.getUnrollFactor();
    unsigned numResults = returnOp.getNumOperands() / unrollFactor;
    assert(returnOp.getNumOperands() % unrollFactor == 0 &&
           "expected number of operands to be a multiple of the unroll factor");
    SmallVector<Value, 10> oldOperands = returnOp.getOperands();
    SmallVector<Value, 10> newOperands;
    SmallVector<Value, 10> newResults;
    for (unsigned i = 0, e = numResults; i != e; ++i) {
      // Get the operands for every result
      auto operands =
          returnOp.getOperands().slice(i * unrollFactor, unrollFactor);
      if (!llvm::all_of(operands, [&](Value value) {
            return llvm::is_contained(newOperands, value);
          })) {
        newOperands.insert(newOperands.end(), operands.begin(), operands.end());
        newResults.push_back(applyOp.getResult(i));
      }
    }

    // Remove duplicates if needed
    if (newOperands.size() < returnOp.getNumOperands()) {
      // Replace the return op
      rewriter.setInsertionPoint(returnOp);
      rewriter.create<stencil::ReturnOp>(returnOp.getLoc(), newOperands,
                                         returnOp.unroll());
      rewriter.eraseOp(returnOp);

      // Clone the apply op
      rewriter.setInsertionPoint(applyOp);
      auto newOp = rewriter.create<stencil::ApplyOp>(
          applyOp.getLoc(), applyOp.getOperands(), newResults);
      rewriter.cloneRegionBefore(applyOp.region(), newOp.region(),
                                 newOp.region().begin());

      // Replace all uses of the applyOp results
      SmallVector<Value, 10> repResults;
      for (size_t i = 0, e = applyOp.getResults().size(); i != e; ++i) {
        auto it = llvm::find(newOperands, oldOperands[i]);
        repResults.push_back(
            newOp.getResult(std::distance(newOperands.begin(), it)));
      }
      rewriter.replaceOp(applyOp, repResults);
      return success();
    }
    return failure();
  }
};

/// This is a pattern to remove duplicate arguments
struct ApplyOpArgCleaner : public OpRewritePattern<stencil::ApplyOp> {
  using OpRewritePattern<stencil::ApplyOp>::OpRewritePattern;

  // Remove duplicates if needed
  LogicalResult matchAndRewrite(stencil::ApplyOp applyOp,
                                PatternRewriter &rewriter) const override {
    // Compute operand list and init the argument matcher
    BlockAndValueMapping mapper;
    SmallVector<Value, 10> newOperands;
    for (unsigned i = 0, e = applyOp.getNumOperands(); i != e; ++i) {
      if (llvm::is_contained(newOperands, applyOp.getOperand(i))) {
        mapper.map(applyOp.getBody()->getArgument(i),
                   applyOp.getBody()->getArgument(i));
      } else {
        newOperands.push_back(applyOp.getOperand(i));
      }
    }

    if (newOperands.size() < applyOp.getNumOperands()) {
      // Replace all uses of duplicates
      for (unsigned i = 0, e = applyOp.getNumOperands(); i != e; ++i) {
        if (mapper.contains(applyOp.getBody()->getArgument(i))) {
          auto it = llvm::find(applyOp.getOperands(), applyOp.getOperand(i));
          size_t index = std::distance(applyOp.getOperands().begin(), it);
          assert(index < i && "expected lower replacement index");
          applyOp.getBody()->getArgument(i).replaceAllUsesWith(
              applyOp.getBody()->getArgument(index));
        }
      }

      // Clone the apply op
      auto loc = applyOp.getLoc();
      auto newOp = rewriter.create<stencil::ApplyOp>(loc, newOperands,
                                                     applyOp.getResults());
      rewriter.cloneRegionBefore(applyOp.region(), newOp.region(),
                                 newOp.region().begin(), mapper);

      // Replace all uses of the applyOp results
      rewriter.replaceOp(applyOp, newOp.getResults());
      return success();
    }
    return failure();
  }
};
} // end anonymous namespace

// TODO write a test and also sort the operations
void stencil::ApplyOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<ApplyOpArgCleaner, ApplyOpResCleaner>(context);
}

namespace mlir {
namespace stencil {

#include "Dialect/Stencil/StencilOpsInterfaces.cpp.inc"

#define GET_OP_CLASSES
#include "Dialect/Stencil/StencilOps.cpp.inc"
} // namespace stencil
} // namespace mlir
