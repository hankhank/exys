//===-- examples/HowToUseJIT/HowToUseJIT.cpp - An example use of the JIT --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This small program provides an example of how to quickly build a small
//  module with two functions and execute it with the JIT.
//
// Goal:
//  The goal of this snippet is to create in the memory
//  the LLVM module consisting of two functions as follow: 
//
// int add1(int x) {
//   return x+1;
// }
//
// int foo() {
//   return add1(10);
// }
//
// then compile the module via JIT, then execute the `foo'
// function and return result to a driver, i.e. to a "host program".
//
// Some remarks and questions:
//
// - could we invoke some code using noname functions too?
//   e.g. evaluate "foo()+foo()" without fears to introduce
//   conflict of temporary function name with some real
//   existing function name?
//
//===----------------------------------------------------------------------===//
//#include <iostream>
//#include <sstream>
//#include <cassert>
//#include <cmath>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

int main() {
  
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

  LLVMContext Context;
  
  // Create some module to put our function into it.
  std::unique_ptr<Module> Owner = make_unique<Module>("test", Context);
  Module *M = Owner.get();

  // Create the add1 function entry and insert this entry into module M.  The
  // function will have a return type of "int" and take an argument of "int".
  // The '0' terminates the list of argument types.
  std::vector<Type*> args;
  args.push_back(Type::getInt32Ty(Context));
  Function *Add1F =
    cast<Function>(M->getOrInsertFunction("add1", FunctionType::get(Type::getInt32Ty(Context), args, false)));

  // Add a basic block to the function. As before, it automatically inserts
  // because of the last argument.
  BasicBlock *BB = BasicBlock::Create(Context, "EntryBlock", Add1F);

  // Create a basic block builder with default parameters.  The builder will
  // automatically append instructions to the basic block `BB'.
  IRBuilder<> builder(BB);

  // Get pointers to the constant `1'.
  Value *One = builder.getInt32(1);

  // Get pointers to the integer argument of the add1 function...
  assert(Add1F->arg_begin() != Add1F->arg_end()); // Make sure there's an arg
  Argument *ArgX = &*Add1F->arg_begin();          // Get the arg
  ArgX->setName("AnArg");            // Give it a nice symbolic name for fun.

  // Create the add instruction, inserting it into the end of BB.
  Value *Add = builder.CreateAdd(One, ArgX);

  // Create the return instruction and add it to the basic block
  builder.CreateRet(Add);

  // Now, function add1 is ready.

  // Now we're going to create function `foo', which returns an int and takes no
  // arguments.
  Function *FooF =
    cast<Function>(M->getOrInsertFunction("foo", Type::getInt32Ty(Context),
                                          nullptr));

  // Add a basic block to the FooF function.
  BB = BasicBlock::Create(Context, "EntryBlock", FooF);

  // Tell the basic block builder to attach itself to the new basic block
  builder.SetInsertPoint(BB);

  // Get pointer to the constant `10'.
  Value *Ten = builder.getInt32(10);

  // Pass Ten to the call to Add1F
  CallInst *Add1CallRes = builder.CreateCall(Add1F, Ten);
  Add1CallRes->setTailCall(true);

  // Create the return instruction and add it to the basic block.
  builder.CreateRet(Add1CallRes);

  // Now we create the JIT.
  ExecutionEngine* EE = EngineBuilder(std::move(Owner)).setEngineKind(llvm::EngineKind::JIT).create();
  int (*foofunc)(void);
  foofunc = (int (*)())EE->getFunctionAddress("foo");
  assert(foofunc);
  EE->finalizeObject();
  foofunc();

  outs() << "We just constructed this LLVM module:\n\n" << *M;

  outs() << "\n\nRunning foo: ";
  outs().flush();

  // Call the `foo' function with no arguments:
  //
  std::vector<GenericValue> noargs;
  //GenericValue gv = EE->runFunction(FooF, noargs);

  // Import result of execution:
  //outs() << "Result: " << gv.IntVal << "\n";

  delete EE;
  llvm_shutdown();
  return 0;
}
#if 0
int main()
{
    llvm::InitializeNativeTarget();
  LLVMContext mLlvmContext;
    auto Owner = std::make_unique<llvm::Module>("exys", mLlvmContext);
    llvm::Module *M = Owner.get();
  
    std::vector<llvm::Type*> args;

    llvm::Function *stabilizeFunc =
    llvm::cast<llvm::Function>(M->getOrInsertFunction("stabilizeFunc", 
                    llvm::FunctionType::get(
                        llvm::Type::getVoidTy(mLlvmContext), // void return
                        args,
                        false))); // no var args

    auto *BB = llvm::BasicBlock::Create(mLlvmContext, "StabilizeBlock", stabilizeFunc);

    llvm::IRBuilder<> builder(BB);

    volatile double test[2];
    test[0] = 100.0;
    test[1] = 0;

    auto* in = llvm::ConstantInt::get(llvm::Type::getInt64Ty(mLlvmContext), (uintptr_t)&test[0]);
    auto* out = llvm::ConstantInt::get(llvm::Type::getInt64Ty(mLlvmContext), (uintptr_t)&test[1]);

    llvm::Value* inptr = llvm::ConstantExpr::getIntToPtr(in, llvm::Type::getDoublePtrTy(mLlvmContext));
    llvm::Value* outptr = llvm::ConstantExpr::getIntToPtr(out, llvm::Type::getDoublePtrTy(mLlvmContext));

    auto* loadIn = builder.CreateLoad(inptr);

    llvm::Value *Add = builder.CreateAdd(loadIn, loadIn);
    llvm::Value *addtwo = builder.CreateAdd(loadIn, Add);

    auto* storeOut = builder.CreateStore(addtwo, outptr);

    builder.CreateRetVoid();

  // Now we create the JIT.
  //EE
  //std::string error; ExecutionEngine *ee = EngineBuilder(module).create();
    std::string error;
  llvm::ExecutionEngine* EE = llvm::EngineBuilder(std::move(Owner)).setErrorStr(&error).create();
  //assert(EE);
  //std::cout << error;
  //llvm::outs() << "We just constructed this LLVM module:\n\n" << *M;

  //std::cout << "\n\nRunning foo: ";

  // Call the `foo' function with no arguments:
  std::vector<llvm::GenericValue> noargs;
  llvm::GenericValue gv = EE->runFunction(stabilizeFunc, noargs);
  //std::cout << test[0];
  //std::cout << "\n";
  //std::cout << (uintptr_t)&test[0];
  //std::cout << "\n";
  //std::cout << test[1];
  //std::cout << "\n";
  //std::cout << (uintptr_t)&test[1];
  //std::cout << "\n";

}
#endif
