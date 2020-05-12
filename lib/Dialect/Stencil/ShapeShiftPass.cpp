#include "Dialect/Stencil/Passes.h"
#include "Dialect/Stencil/StencilDialect.h"
#include "Dialect/Stencil/StencilOps.h"
#include "Dialect/Stencil/StencilTypes.h"
#include "PassDetail.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/STLExtras.h"
#include <bits/stdint-intn.h>
#include <cstddef>
#include <limits>

using namespace mlir;
using namespace stencil;

namespace {

// Helper method to shift a bound
Index shiftOffset(ArrayRef<int64_t> bound,
                                    ArrayRef<int64_t> shift) {
  assert(bound.size() == shift.size() &&
         "expected bound and shift to have the same size");
  Index result(bound.size());
  llvm::transform(llvm::zip(bound, shift), result.begin(),
                  [](std::tuple<int64_t, int64_t> x) {
                    return std::get<0>(x) - std::get<1>(x);
                  });


  return result;
}

struct ShapeShiftPass : public ShapeShiftPassBase<ShapeShiftPass> {
  void runOnFunction() override;
};

} // namespace

void ShapeShiftPass::runOnFunction() {
  FuncOp funcOp = getFunction();

  // Only run on functions marked as stencil programs
  if (!stencil::StencilDialect::isStencilProgram(funcOp))
    return;

  // // Verify all apply and load ops have valid bounds
  // bool invalidBounds = false;
  // funcOp.walk([&](ShapeOp ShapeOp) {
  //   if (!ShapeOp.hasShape()) {
  //     ShapeOp.emitOpError("expected op to have valid bounds");
  //     invalidBounds = true;
  //   }
  // });
  // if (invalidBounds)
  //   return signalPassFailure();

  // // Adapt the access offsets to the positive range
  // funcOp.walk([&](stencil::ApplyOp applyOp) {
  //   // Get the output bound
  //   Index output = applyOp.getLB();
  //   // Get the input bound of the operand
  //   for (unsigned i = 0, e = applyOp.getNumOperands(); i != e; ++i) {
  //     Index input;
  //     // Compute the shift for the operand
  //     auto operand = applyOp.getOperand(i);
  //     if (auto loadOp =
  //             dyn_cast_or_null<stencil::LoadOp>(operand.getDefiningOp())) {
  //       input = loadOp.getLB();
  //     }
  //     if (auto applyOp =
  //             dyn_cast_or_null<stencil::ApplyOp>(operand.getDefiningOp())) {
  //       input = applyOp.getLB();
  //     }
  //     // Shift all accesses of the corresponding operand
  //     auto argument = applyOp.getBody()->getArgument(i);
  //     applyOp.walk([&](stencil::AccessOp accessOp) {
  //       if (accessOp.temp() == argument)
  //         accessOp.setOffset(
  //             shiftOffset(accessOp.getOffset(), shiftOffset(input, output)));
  //     });
  //   }
  // });

  // // Adapt the loop bounds of all apply ops to start start at zero
  // funcOp.walk([](stencil::ApplyOp applyOp) {
  //   Index shift = applyOp.getLB();
  //   applyOp.setLB(shiftOffset(applyOp.getLB(), shift));
  //   applyOp.setUB(shiftOffset(applyOp.getUB(), shift));
  // });

  // // Adapt the bounds for all loads and stores
  // funcOp.walk([](stencil::AssertOp assertOp) {
  //   // Adapt bounds by the lower bound of the assert op
  //   Index shift = assertOp.getLB();
  //   for (auto &use : assertOp.field().getUses()) {
  //     if (auto loadOp = dyn_cast<stencil::LoadOp>(use.getOwner())) {
  //       loadOp.setLB(shiftOffset(loadOp.getLB(), shift));
  //       loadOp.setUB(shiftOffset(loadOp.getUB(), shift));
  //     }
  //     if (auto storeOp = dyn_cast<stencil::StoreOp>(use.getOwner())) {
  //       storeOp.setLB(shiftOffset(storeOp.getLB(), shift));
  //       storeOp.setUB(shiftOffset(storeOp.getUB(), shift));
  //     }
  //   }

  //   // Adapt the assert bounds
  //   assertOp.setLB(shiftOffset(assertOp.getLB(), shift));
  //   assertOp.setUB(shiftOffset(assertOp.getUB(), shift));
  // });

  // // Update bounds of lower dimensional fields
  // funcOp.walk([](Operation *op) {
  //   if (auto accessOp = dyn_cast<stencil::AccessOp>(op)) {
  //     accessOp.setOffset(
  //         markIgnoredDimensions(accessOp.temp(), accessOp.getOffset()));
  //   }
  //   if (auto loadOp = dyn_cast<stencil::LoadOp>(op)) {
  //     loadOp.setLB(markIgnoredDimensions(loadOp.res(), loadOp.getLB()));
  //     loadOp.setUB(markIgnoredDimensions(loadOp.res(), loadOp.getUB()));
  //   }
  //   if (auto storeOp = dyn_cast<stencil::StoreOp>(op)) {
  //     storeOp.setLB(markIgnoredDimensions(storeOp.field(), storeOp.getLB()));
  //     storeOp.setUB(markIgnoredDimensions(storeOp.field(), storeOp.getUB()));
  //   }
  //   if (auto assertOp = dyn_cast<stencil::AssertOp>(op)) {
  //     assertOp.setLB(markIgnoredDimensions(assertOp.field(), assertOp.getLB()));
  //     assertOp.setUB(markIgnoredDimensions(assertOp.field(), assertOp.getUB()));
  //   }
  // });
}

std::unique_ptr<OperationPass<FuncOp>> mlir::createShapeShiftPass() {
  return std::make_unique<ShapeShiftPass>();
}