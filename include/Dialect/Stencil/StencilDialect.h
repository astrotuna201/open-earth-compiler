#ifndef MLIR_DIALECT_STENCIL_STENCILDIALECT_H
#define MLIR_DIALECT_STENCIL_STENCILDIALECT_H

#include "mlir/IR/Dialect.h"
#include "mlir/IR/Function.h"

namespace mlir {
namespace stencil {

// Constant used to mark unused dimensions of lower dimensional fields
constexpr static int64_t kIgnoreDimension = std::numeric_limits<int64_t>::min();

// Constant dimension identifiers
constexpr static int kIDimension = 0;
constexpr static int kJDimension = 1;
constexpr static int kKDimension = 2;

// Constant used to define the loop unrolling dimension
constexpr static int kUnrollDimension = kJDimension;

class StencilDialect : public Dialect {
public:
  explicit StencilDialect(MLIRContext *context);

  /// Returns the prefix used in the textual IR to refer to stencil operations
  static StringRef getDialectNamespace() { return "stencil"; }

  static StringRef getStencilFunctionAttrName() { return "stencil.function"; }
  static StringRef getStencilProgramAttrName() { return "stencil.program"; }

  static StringRef getFieldTypeName() { return "field"; }
  static StringRef getViewTypeName() { return "view"; }

  static bool isStencilFunction(FuncOp funcOp) {
    return !!funcOp.getAttr(getStencilFunctionAttrName());
  }
  static bool isStencilProgram(FuncOp funcOp) {
    return !!funcOp.getAttr(getStencilProgramAttrName());
  }

  /// Parses a type registered to this dialect
  Type parseType(DialectAsmParser &parser) const override;

  /// Print a type registered to this dialect
  void printType(Type type, DialectAsmPrinter &os) const override;
};

} // namespace stencil
} // namespace mlir

#endif // MLIR_DIALECT_STENCIL_STENCILDIALECT_H
