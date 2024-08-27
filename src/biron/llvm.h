#ifndef BIRON_LLVM_H
#define BIRON_LLVM_H
#include <biron/util/maybe.inl>
#include <biron/util/types.inl>

namespace Biron {

struct LLVM {
	LLVM();
	~LLVM();

	struct OpaqueContext;
	struct OpaqueModule;
	struct OpaqueType;
	struct OpaqueValue;
	struct OpaqueBasicBlock;
	struct OpaqueBuilder;
	struct OpaqueTargetMachineOptions;
	struct OpaqueTargetMachine;
	struct OpaqueTarget;
	struct OpaquePassBuilderOptions;
	struct OpaqueError;

	using ContextRef              = OpaqueContext*;
	using ModuleRef               = OpaqueModule*;
	using TypeRef                 = OpaqueType*;
	using ValueRef                = OpaqueValue*;
	using BasicBlockRef           = OpaqueBasicBlock*;
	using BuilderRef              = OpaqueBuilder*;
	using TargetMachineOptionsRef = OpaqueTargetMachineOptions*;
	using TargetMachineRef        = OpaqueTargetMachine*;
	using TargetRef               = OpaqueTarget*;
	using PassBuilderOptionsRef   = OpaquePassBuilderOptions*;
	using ErrorRef                = OpaqueError*;
	using Bool                    = int;
	using Ulen                    = decltype(sizeof 0);

	enum class CodeGenOptLevel : int {
		None,
		Less,
		Default,
		Aggressive
	};

	enum class RelocMode : int {
		Default,
		Static,
		PIC,
		DynamicNoPic,
		ROPI,
		RWPI,
		ROPI_RWPI,
	};

	enum class CodeModel : int {
		Default,
		JITDefault,
		Tiny,
		Small,
		Kernel,
		Medium,
		Large
	};

	enum class CodeGenFileType : int {
		Assembly,
		Object
	};

	enum class VerifierFailureAction : int {
		AbortProcess,
		PrintMessage,
		ReturnStatus 
	};

	enum class IntPredicate : int {
		EQ = 32, /**< equal */
		NE,      /**< not equal */
		UGT,     /**< unsigned greater than */
		UGE,     /**< unsigned greater or equal */
		ULT,     /**< unsigned less than */
		ULE,     /**< unsigned less or equal */
		SGT,     /**< signed greater than */
		SGE,     /**< signed greater or equal */
		SLT,     /**< signed less than */
		SLE      /**< signed less or equal */
	};

	using Opcode = int;

	// Global
	void (*InitializeX86TargetInfo)(void);
	void (*InitializeX86Target)(void);
	void (*InitializeX86TargetMC)(void);
	void (*InitializeX86AsmPrinter)(void);
	void (*InitializeX86AsmParser)(void);
	void (*DisposeMessage)(char *);

	void (*Shutdown)(void);

	// Context
	ContextRef (*ContextCreate)(void);
	void (*ContextDispose)(ContextRef);
	TypeRef (*GetTypeByName2)(ContextRef, const char*);

	TypeRef (*Int1TypeInContext)(ContextRef); // Boolean
	TypeRef (*Int8TypeInContext)(ContextRef); // Uint8, Sint8
	TypeRef (*Int16TypeInContext)(ContextRef); // Uint16, Sint16
	TypeRef (*Int32TypeInContext)(ContextRef); // Uint32, Sint32
	TypeRef (*Int64TypeInContext)(ContextRef); // Uint64, Sint64
	TypeRef (*VoidTypeInContext)(ContextRef); // ()
	TypeRef (*FloatTypeInContext)(ContextRef);
	TypeRef (*DoubleTypeInContext)(ContextRef);
	TypeRef (*PointerTypeInContext)(ContextRef, unsigned); // *T
	TypeRef (*StructTypeInContext)(ContextRef, TypeRef*, unsigned, Bool); // struct{..}, (...)
	TypeRef (*StructCreateNamed)(ContextRef, const char*);
	void (*StructSetBody)(TypeRef, TypeRef*, unsigned, Bool);
	TypeRef (*ArrayType2)(TypeRef, Uint64); // [N]T
	TypeRef (*StructGetTypeAtIndex)(TypeRef, unsigned); // ...?
	TypeRef (*FunctionType)(TypeRef, TypeRef*, unsigned, Bool);

	BasicBlockRef (*CreateBasicBlockInContext)(ContextRef, const char *);

	// BasicBlock
	ValueRef (*GetBasicBlockParent)(BasicBlockRef);
	ValueRef (*GetBasicBlockTerminator)(BasicBlockRef);
	ValueRef (*ConstInt)(TypeRef, unsigned long long, Bool);
	ValueRef (*ConstReal)(TypeRef, Float64);
	ValueRef (*ConstArray2)(TypeRef, ValueRef*, Uint64);
	ValueRef (*ConstPointerNull)(TypeRef);
	ValueRef (*ConstStructInContext)(ContextRef, ValueRef*, unsigned, Bool);
	ValueRef (*ConstNamedStruct)(TypeRef, ValueRef*, unsigned);
	ValueRef (*GetAggregateElement)(ValueRef, unsigned);
	unsigned (*CountStructElementTypes)(TypeRef);

	// Value
	void (*AppendExistingBasicBlock)(ValueRef, BasicBlockRef);
	ValueRef (*GetParam)(ValueRef, unsigned);
	void (*SetGlobalConstant)(ValueRef, Bool);
	ValueRef (*AddGlobal)(ModuleRef, TypeRef, const char*);
	void (*SetInitializer)(ValueRef, ValueRef);
	void (*SetSection)(ValueRef, const char *);
	void (*SetAlignment)(ValueRef, unsigned);
	void (*SetValueName2)(ValueRef, const char*, Ulen);

	// Module
	ModuleRef (*ModuleCreateWithNameInContext)(const char *, ContextRef);
	ValueRef (*AddFunction)(ModuleRef, const char *, TypeRef);
	ValueRef (*GetNamedFunction)(ModuleRef, const char *);
	void (*DisposeModule)(ModuleRef);
	void (*DumpModule)(ModuleRef);
	Bool (*VerifyModule)(ModuleRef M, VerifierFailureAction, char **);

	// Builder
	BuilderRef (*CreateBuilderInContext)(ContextRef);
	void (*DisposeBuilder)(BuilderRef);
	void (*PositionBuilderAtEnd)(BuilderRef, BasicBlockRef);
	BasicBlockRef (*GetInsertBlock)(BuilderRef);
	ValueRef (*BuildRet)(BuilderRef, ValueRef);
	ValueRef (*BuildRetVoid)(BuilderRef);
	ValueRef (*BuildBr)(BuilderRef, BasicBlockRef);
	ValueRef (*BuildAdd)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildFAdd)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildSub)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildFSub)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildMul)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildFMul)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildFDiv)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildAnd)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildOr)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildShl)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildLShr)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildAShr)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildXor)(BuilderRef, ValueRef, ValueRef, const char*);
	ValueRef (*BuildNeg)(BuilderRef, ValueRef, const char *);
	ValueRef (*BuildNot)(BuilderRef, ValueRef, const char *);
	ValueRef (*BuildCall2)(BuilderRef, TypeRef, ValueRef, ValueRef*, unsigned, const char *);
	ValueRef (*BuildSelect)(BuilderRef, ValueRef, ValueRef, ValueRef, const char *);
	ValueRef (*BuildCondBr)(BuilderRef, ValueRef, BasicBlockRef, BasicBlockRef);
	ValueRef (*BuildLoad2)(BuilderRef, TypeRef, ValueRef, const char *);
	void (*DumpType)(TypeRef);
	ValueRef (*BuildStore)(BuilderRef, ValueRef, ValueRef);
	ValueRef (*BuildGEP2)(BuilderRef, TypeRef, ValueRef, ValueRef *, unsigned, const char *);
	ValueRef (*BuildGlobalString)(BuilderRef B, const char *, const char *);
	ValueRef (*BuildICmp)(BuilderRef, IntPredicate, ValueRef, ValueRef, const char *);
	ValueRef (*BuildAlloca)(BuilderRef, TypeRef, const char *);
	ValueRef (*BuildCast)(BuilderRef, Opcode, ValueRef, TypeRef, const char *);
	ValueRef (*BuildExtractValue)(BuilderRef, ValueRef, unsigned, const char *);
	Opcode (*GetCastOpcode)(ValueRef, Bool, TypeRef, Bool);
	ValueRef (*BuildPhi)(BuilderRef, TypeRef, const char*);
	void (*AddIncoming)(ValueRef, ValueRef*, BasicBlockRef*, unsigned);

	// Target
	Bool (*GetTargetFromTriple)(const char*, TargetRef *, char **);

	// Target Machine
	TargetMachineRef (*CreateTargetMachine)(TargetRef, const char *, const char *, const char *, CodeGenOptLevel, RelocMode, CodeModel);
	void (*DisposeTargetMachine)(TargetMachineRef);
	Bool (*TargetMachineEmitToFile)(TargetMachineRef, ModuleRef, const char *, CodeGenFileType codegen, char **);

	// Pass builder
	PassBuilderOptionsRef (*CreatePassBuilderOptions)(void);
	void (*DisposePassBuilderOptions)(PassBuilderOptionsRef);
	ErrorRef (*RunPasses)(ModuleRef, const char *, TargetMachineRef, PassBuilderOptionsRef);

	void* lib;

	static Maybe<LLVM> load() noexcept;
};

} // namespace Biron

#endif // BIRON_LLVM_H