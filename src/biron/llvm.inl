#ifndef FN
#define FN(...)
#endif

// This file declares functions for LLVM in the same order as
//  Analysis.h
//  Core.h
//  Error.h
//  Target.h
//  TargetMachine.h
//  PassBuilder.h
//
// We just omit functions we do not use in Biron

//
// Analysis.h
//
FN(Bool,                  VerifyModule,                  ModuleRef, VerifierFailureAction, char**)

//
// Core.h
//

// Global
FN(void,                  Shutdown,                      void)
FN(void,                  GetVersion,                    unsigned*, unsigned*, unsigned*)
FN(void,                  DisposeMessage,                char*)
// Contexts
FN(ContextRef,            ContextCreate,                 void)
FN(void,                  ContextDispose,                ContextRef)
FN(TypeRef,               GetTypeByName2,                ContextRef, const char*)
FN(unsigned,              GetEnumAttributeKindForName,   const char*, Ulen)
FN(AttributeRef,          CreateEnumAttribute,           ContextRef, unsigned, Uint64)
// Modules
FN(ModuleRef,             ModuleCreateWithNameInContext, const char*, ContextRef)
FN(void,                  DisposeModule,                 ModuleRef)
FN(void,                  DumpModule,                    ModuleRef)
FN(ValueRef,              AddFunction,                   ModuleRef, const char*, TypeRef)
// Types
/// Integer Types
FN(TypeRef,               Int1TypeInContext,             ContextRef)
FN(TypeRef,               Int8TypeInContext,             ContextRef)
FN(TypeRef,               Int16TypeInContext,            ContextRef)
FN(TypeRef,               Int32TypeInContext,            ContextRef)
FN(TypeRef,               Int64TypeInContext,            ContextRef)
/// Floating Point Types
FN(TypeRef,               FloatTypeInContext,            ContextRef)
FN(TypeRef,               DoubleTypeInContext,           ContextRef)
/// Function Types
FN(TypeRef,               FunctionType,                  TypeRef, TypeRef*, unsigned, Bool) // No InContext version
/// Structure Types
FN(TypeRef,               StructTypeInContext,           ContextRef, TypeRef*, unsigned, Bool)
FN(TypeRef,               StructCreateNamed,             ContextRef, const char*)
FN(void,                  StructSetBody,                 TypeRef, TypeRef*, unsigned, Bool)
FN(Bool,                  IsLiteralStruct,               TypeRef)
/// Sequential Types
FN(TypeRef,               ArrayType2,                    TypeRef, Uint64)
/// Other Types
FN(TypeRef,               PointerTypeInContext,          ContextRef, unsigned)
FN(TypeRef,               VoidTypeInContext,             ContextRef)
// Values
/// General APIs
FN(void,                  SetValueName2,                 ValueRef, const char*, Ulen)
/// Constants
FN(ValueRef,              ConstNull,                     TypeRef)
FN(ValueRef,              ConstPointerNull,              TypeRef)
//// Scalar Constants
FN(ValueRef,              ConstInt,                      TypeRef, unsigned long long, Bool)
FN(ValueRef,              ConstReal,                     TypeRef, double)
//// Composite Constants
// FN(ValueRef,              ConstStringInContext2,         ContextRef, const char*, Ulen, Bool)
FN(ValueRef,              ConstStructInContext,          ContextRef, ValueRef*, unsigned, Bool)
FN(ValueRef,              ConstArray2,                   TypeRef, ValueRef*, Uint64)
FN(ValueRef,              ConstNamedStruct,              TypeRef, ValueRef*, unsigned)
/// Global Values
FN(void,                  SetSection,                    ValueRef, const char*)
FN(void,                  SetAlignment,                  ValueRef, unsigned)
/// Global Variables
FN(ValueRef,              AddGlobal,                     ModuleRef, TypeRef, const char*)
FN(void,                  SetInitializer,                ValueRef, ValueRef)
/// Function Values
FN(void,                  AddAttributeAtIndex,           ValueRef, AttributeIndex, AttributeRef)
/// Function Parameters
FN(ValueRef,              GetParam,                      ValueRef, unsigned)
// Basic Block
FN(ValueRef,              GetBasicBlockParent,           BasicBlockRef)
FN(ValueRef,              GetBasicBlockTerminator,       BasicBlockRef)
FN(BasicBlockRef,         CreateBasicBlockInContext,     ContextRef, const char*)
FN(void,                  AppendExistingBasicBlock,      ValueRef, BasicBlockRef)
// PHI Nodes
FN(void,                  AddIncoming,                   ValueRef, ValueRef*, BasicBlockRef*, unsigned)
// Instruction Builders
FN(BuilderRef,            CreateBuilderInContext,        ContextRef)
FN(void,                  PositionBuilderAtEnd,          BuilderRef, BasicBlockRef)
FN(BasicBlockRef,         GetInsertBlock,                BuilderRef)
FN(void,                  DisposeBuilder,                BuilderRef)
/// Terminators
FN(ValueRef,              BuildRetVoid,                  BuilderRef)
FN(ValueRef,              BuildRet,                      BuilderRef, ValueRef)
FN(ValueRef,              BuildBr,                       BuilderRef, BasicBlockRef)
FN(ValueRef,              BuildCondBr,                   BuilderRef, ValueRef, BasicBlockRef, BasicBlockRef)
/// Arithmetic
FN(ValueRef,              BuildAdd,                      BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFAdd,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildSub,                      BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFSub,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildMul,                      BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFMul,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildUDiv,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildSDiv,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFDiv,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildURem,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildSRem,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFRem,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildShl,                      BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildLShr,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildAShr,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildAnd,                      BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildOr,                       BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildXor,                      BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildNeg,                      BuilderRef, ValueRef, const char*)
FN(ValueRef,              BuildFNeg,                     BuilderRef, ValueRef, const char*)
FN(ValueRef,              BuildNot,                      BuilderRef, ValueRef, const char*)
/// Memory
FN(ValueRef,              BuildAlloca,                   BuilderRef, TypeRef, const char*);
FN(ValueRef,              BuildLoad2,                    BuilderRef, TypeRef, ValueRef, const char*)
FN(ValueRef,              BuildStore,                    BuilderRef, ValueRef, ValueRef)
FN(ValueRef,              BuildGEP2,                     BuilderRef, TypeRef, ValueRef, ValueRef*, unsigned, const char*)
FN(ValueRef,              BuildGlobalString,             BuilderRef, const char*, const char*)
/// Casts
FN(ValueRef,              BuildCast,                     BuilderRef, Opcode, ValueRef, TypeRef, const char*)
FN(Opcode,                GetCastOpcode,                 ValueRef, Bool, TypeRef, Bool)
/// Comparisons
FN(ValueRef,              BuildICmp,                     BuilderRef, IntPredicate, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFCmp,                     BuilderRef, RealPredicate, ValueRef, ValueRef, const char*)
/// Miscellaneous
FN(ValueRef,              BuildPhi,                      BuilderRef, TypeRef, const char*)
FN(ValueRef,              BuildCall2,                    BuilderRef, TypeRef, ValueRef, ValueRef*, unsigned, const char*)
FN(ValueRef,              BuildExtractValue,             BuilderRef, ValueRef, unsigned, const char*)

//
// Error.h
//
FN(void,                  ConsumeError,                  ErrorRef)

//
// Target.h
//
FN(void,                  InitializeX86TargetInfo,       void)
FN(void,                  InitializeX86Target,           void)
FN(void,                  InitializeX86TargetMC,         void)
FN(void,                  InitializeX86AsmPrinter,       void)
FN(void,                  InitializeX86AsmParser,        void)

//
// TargetMachine.h
//
FN(Bool,                  GetTargetFromTriple,           const char*, TargetRef*, char**)
FN(TargetMachineRef,      CreateTargetMachine,           TargetRef, const char*, const char*, const char*, CodeGenOptLevel, RelocMode, CodeModel)
FN(void,                  DisposeTargetMachine,          TargetMachineRef)
FN(Bool,                  TargetMachineEmitToFile,       TargetMachineRef, ModuleRef, const char*, CodeGenFileType, char**)

//
// PassBuilder.h
//
FN(ErrorRef,              RunPasses,                     ModuleRef, const char*, TargetMachineRef, PassBuilderOptionsRef)
FN(PassBuilderOptionsRef, CreatePassBuilderOptions,      void)
FN(void,                  DisposePassBuilderOptions,     PassBuilderOptionsRef)