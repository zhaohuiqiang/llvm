//=- SyntheticCountsPropagation.cpp - Propagate function counts --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a transformation that synthesizes entry counts for
// functions and attaches !prof metadata to functions with the synthesized
// counts. The presence of !prof metadata with counter name set to
// 'synthesized_function_entry_count' indicate that the value of the counter is
// an estimation of the likely execution count of the function. This transform
// is applied only in non PGO mode as functions get 'real' profile-based
// function entry counts in the PGO mode.
//
// The transformation works by first assigning some initial values to the entry
// counts of all functions and then doing a top-down traversal of the
// callgraph-scc to propagate the counts. For each function the set of callsites
// and their relative block frequency is gathered. The relative block frequency
// multiplied by the entry count of the caller and added to the callee's entry
// count. For non-trivial SCCs, the new counts are computed from the previous
// counts and updated in one shot.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/SyntheticCountsPropagation.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/SyntheticCountsUtils.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using Scaled64 = ScaledNumber<uint64_t>;

#define DEBUG_TYPE "synthetic-counts-propagation"

/// Initial synthetic count assigned to functions.
static cl::opt<int>
    InitialSyntheticCount("initial-synthetic-count", cl::Hidden, cl::init(10),
                          cl::ZeroOrMore,
                          cl::desc("Initial value of synthetic entry count."));

/// Initial synthetic count assigned to inline functions.
static cl::opt<int> InlineSyntheticCount(
    "inline-synthetic-count", cl::Hidden, cl::init(15), cl::ZeroOrMore,
    cl::desc("Initial synthetic entry count for inline functions."));

/// Initial synthetic count assigned to cold functions.
static cl::opt<int> ColdSyntheticCount(
    "cold-synthetic-count", cl::Hidden, cl::init(5), cl::ZeroOrMore,
    cl::desc("Initial synthetic entry count for cold functions."));

// Assign initial synthetic entry counts to functions.
static void
initializeCounts(Module &M, function_ref<void(Function *, uint64_t)> SetCount) {
  auto MayHaveIndirectCalls = [](Function &F) {
    for (auto *U : F.users()) {
      if (!isa<CallInst>(U) && !isa<InvokeInst>(U))
        return true;
    }
    return false;
  };

  for (Function &F : M) {
    uint64_t InitialCount = InitialSyntheticCount;
    if (F.isDeclaration())
      continue;
    if (F.hasFnAttribute(Attribute::AlwaysInline) ||
        F.hasFnAttribute(Attribute::InlineHint)) {
      // Use a higher value for inline functions to account for the fact that
      // these are usually beneficial to inline.
      InitialCount = InlineSyntheticCount;
    } else if (F.hasLocalLinkage() && !MayHaveIndirectCalls(F)) {
      // Local functions without inline hints get counts only through
      // propagation.
      InitialCount = 0;
    } else if (F.hasFnAttribute(Attribute::Cold) ||
               F.hasFnAttribute(Attribute::NoInline)) {
      // Use a lower value for noinline and cold functions.
      InitialCount = ColdSyntheticCount;
    }
    SetCount(&F, InitialCount);
  }
}

PreservedAnalyses SyntheticCountsPropagation::run(Module &M,
                                                  ModuleAnalysisManager &MAM) {
  FunctionAnalysisManager &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  DenseMap<Function *, uint64_t> Counts;
  // Set initial entry counts.
  initializeCounts(M, [&](Function *F, uint64_t Count) { Counts[F] = Count; });

  // Compute the relative block frequency for a callsite. Use scaled numbers
  // and not integers since the relative block frequency could be less than 1.
  auto GetCallSiteRelFreq = [&](CallSite CS) {
    Function *Caller = CS.getCaller();
    auto &BFI = FAM.getResult<BlockFrequencyAnalysis>(*Caller);
    BasicBlock *CSBB = CS.getInstruction()->getParent();
    Scaled64 EntryFreq(BFI.getEntryFreq(), 0);
    Scaled64 BBFreq(BFI.getBlockFreq(CSBB).getFrequency(), 0);
    BBFreq /= EntryFreq;
    return BBFreq;
  };

  CallGraph CG(M);
  // Propgate the entry counts on the callgraph.
  propagateSyntheticCounts(
      CG, GetCallSiteRelFreq, [&](Function *F) { return Counts[F]; },
      [&](Function *F, uint64_t New) { Counts[F] += New; });

  // Set the counts as metadata.
  for (auto Entry : Counts)
    Entry.first->setEntryCount(Entry.second, true);

  return PreservedAnalyses::all();
}
