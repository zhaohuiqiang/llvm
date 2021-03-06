//===- Wasm.h - Wasm object file format -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines manifest constants for the wasm object file format.
// See: https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BINARYFORMAT_WASM_H
#define LLVM_BINARYFORMAT_WASM_H

#include "llvm/ADT/ArrayRef.h"

namespace llvm {
namespace wasm {

// Object file magic string.
const char WasmMagic[] = {'\0', 'a', 's', 'm'};
// Wasm binary format version
const uint32_t WasmVersion = 0x1;
// Wasm uses a 64k page size
const uint32_t WasmPageSize = 65536;

struct WasmObjectHeader {
  StringRef Magic;
  uint32_t Version;
};

struct WasmSignature {
  std::vector<int32_t> ParamTypes;
  int32_t ReturnType;
};

struct WasmExport {
  StringRef Name;
  uint32_t Kind;
  uint32_t Index;
};

struct WasmLimits {
  uint32_t Flags;
  uint32_t Initial;
  uint32_t Maximum;
};

struct WasmTable {
  int32_t ElemType;
  WasmLimits Limits;
};

struct WasmInitExpr {
  uint8_t Opcode;
  union {
    int32_t Int32;
    int64_t Int64;
    int32_t Float32;
    int64_t Float64;
    uint32_t Global;
  } Value;
};

struct WasmGlobal {
  uint32_t Index;
  int32_t Type;
  bool Mutable;
  WasmInitExpr InitExpr;
};

struct WasmImport {
  StringRef Module;
  StringRef Field;
  uint32_t Kind;
  union {
    uint32_t SigIndex;
    WasmGlobal Global;
    WasmTable Table;
    WasmLimits Memory;
  };
};

struct WasmLocalDecl {
  int32_t Type;
  uint32_t Count;
};

struct WasmFunction {
  uint32_t Index;
  std::vector<WasmLocalDecl> Locals;
  ArrayRef<uint8_t> Body;
  uint32_t CodeSectionOffset;
  uint32_t Size;
  StringRef Comdat;
};

struct WasmDataSegment {
  uint32_t MemoryIndex;
  WasmInitExpr Offset;
  ArrayRef<uint8_t> Content;
  StringRef Name;
  uint32_t Alignment;
  uint32_t Flags;
  StringRef Comdat;
};

struct WasmElemSegment {
  uint32_t TableIndex;
  WasmInitExpr Offset;
  std::vector<uint32_t> Functions;
};

struct WasmRelocation {
  uint32_t Type;   // The type of the relocation.
  uint32_t Index;  // Index into function to global index space.
  uint64_t Offset; // Offset from the start of the section.
  int64_t Addend;  // A value to add to the symbol.
};

struct WasmInitFunc {
  uint32_t Priority;
  uint32_t FunctionIndex;
};

struct WasmLinkingData {
  uint32_t DataSize;
  std::vector<WasmInitFunc> InitFunctions;
};

enum : unsigned {
  WASM_SEC_CUSTOM = 0,   // Custom / User-defined section
  WASM_SEC_TYPE = 1,     // Function signature declarations
  WASM_SEC_IMPORT = 2,   // Import declarations
  WASM_SEC_FUNCTION = 3, // Function declarations
  WASM_SEC_TABLE = 4,    // Indirect function table and other tables
  WASM_SEC_MEMORY = 5,   // Memory attributes
  WASM_SEC_GLOBAL = 6,   // Global declarations
  WASM_SEC_EXPORT = 7,   // Exports
  WASM_SEC_START = 8,    // Start function declaration
  WASM_SEC_ELEM = 9,     // Elements section
  WASM_SEC_CODE = 10,    // Function bodies (code)
  WASM_SEC_DATA = 11     // Data segments
};

// Type immediate encodings used in various contexts.
enum {
  WASM_TYPE_I32 = -0x01,
  WASM_TYPE_I64 = -0x02,
  WASM_TYPE_F32 = -0x03,
  WASM_TYPE_F64 = -0x04,
  WASM_TYPE_ANYFUNC = -0x10,
  WASM_TYPE_FUNC = -0x20,
  WASM_TYPE_NORESULT = -0x40, // for blocks with no result values
};

// Kinds of externals (for imports and exports).
enum : unsigned {
  WASM_EXTERNAL_FUNCTION = 0x0,
  WASM_EXTERNAL_TABLE = 0x1,
  WASM_EXTERNAL_MEMORY = 0x2,
  WASM_EXTERNAL_GLOBAL = 0x3,
};

// Opcodes used in initializer expressions.
enum : unsigned {
  WASM_OPCODE_END = 0x0b,
  WASM_OPCODE_GET_GLOBAL = 0x23,
  WASM_OPCODE_I32_CONST = 0x41,
  WASM_OPCODE_I64_CONST = 0x42,
  WASM_OPCODE_F32_CONST = 0x43,
  WASM_OPCODE_F64_CONST = 0x44,
};

enum : unsigned {
  WASM_LIMITS_FLAG_HAS_MAX = 0x1,
};

// Subset of types that a value can have
enum class ValType {
  I32 = WASM_TYPE_I32,
  I64 = WASM_TYPE_I64,
  F32 = WASM_TYPE_F32,
  F64 = WASM_TYPE_F64,
};

// Kind codes used in the custom "name" section
enum : unsigned {
  WASM_NAMES_FUNCTION = 0x1,
  WASM_NAMES_LOCAL    = 0x2,
};

// Kind codes used in the custom "linking" section
enum : unsigned {
  WASM_SYMBOL_INFO    = 0x2,
  WASM_DATA_SIZE      = 0x3,
  WASM_SEGMENT_INFO   = 0x5,
  WASM_INIT_FUNCS     = 0x6,
  WASM_COMDAT_INFO    = 0x7,
};

// Kind codes used in the custom "linking" section in the WASM_COMDAT_INFO
enum : unsigned {
  WASM_COMDAT_DATA        = 0x0,
  WASM_COMDAT_FUNCTION    = 0x1,
};

const unsigned WASM_SYMBOL_BINDING_MASK       = 0x3;
const unsigned WASM_SYMBOL_VISIBILITY_MASK    = 0x4;

const unsigned WASM_SYMBOL_BINDING_GLOBAL     = 0x0;
const unsigned WASM_SYMBOL_BINDING_WEAK       = 0x1;
const unsigned WASM_SYMBOL_BINDING_LOCAL      = 0x2;
const unsigned WASM_SYMBOL_VISIBILITY_DEFAULT = 0x0;
const unsigned WASM_SYMBOL_VISIBILITY_HIDDEN  = 0x4;

#define WASM_RELOC(name, value) name = value,

enum : unsigned {
#include "WasmRelocs.def"
};

#undef WASM_RELOC

struct Global {
  ValType Type;
  bool Mutable;

  // The initial value for this global is either the value of an imported
  // global, in which case InitialModule and InitialName specify the global
  // import, or a value, in which case InitialModule is empty and InitialValue
  // holds the value.
  StringRef InitialModule;
  StringRef InitialName;
  uint64_t InitialValue;
};

} // end namespace wasm
} // end namespace llvm

#endif
