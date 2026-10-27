#pragma once

#include "CoreMinimal.h"

/**
 * All or nothing: each step is applied with its undo. When a step fails, every
 * step before it is undone in reverse order. An undo reports whether it worked;
 * if any undo fails the world may be inconsistent, and RollbackFailed() says so
 * (the kernel then marks the simulation faulted instead of carrying on).
 * A transaction that is neither committed nor rolled back rolls back when it
 * goes out of scope.
 *
 *   FPlacementTransaction Tx;
 *   Tx.Step(TEXT("Graph"), [&] { return Graph->TryConnect(...); }, [&] { return Graph->TryDisconnect(...); });
 *   ...
 *   if (Tx.Failed()) return false;
 *   Tx.Commit();
 */
class FPlacementTransaction
{
public:
    FPlacementTransaction() = default;
    FPlacementTransaction(const FPlacementTransaction&) = delete;
    FPlacementTransaction& operator=(const FPlacementTransaction&) = delete;

    ~FPlacementTransaction()
    {
        if (!bFinished)
            Rollback();
    }

    /** Runs Apply; on failure undoes everything so far. Does nothing after a failure. */
    bool Step(const TCHAR* Name, TFunctionRef<bool()> Apply, TFunction<bool()> Undo = nullptr)
    {
        if (bFailed || bFinished)
            return false;
        if (!Apply())
        {
            bFailed = true;
            FailedStep = Name;
            Rollback();
            return false;
        }
        if (Undo)
            Undos.Add(TPair<FString, TFunction<bool()>>(Name, MoveTemp(Undo)));
        return true;
    }

    /** Fails the transaction on purpose (a check outside a step). */
    void Fail(const TCHAR* Name)
    {
        if (bFailed || bFinished)
            return;
        bFailed = true;
        FailedStep = Name;
        Rollback();
    }

    void Commit()
    {
        if (!bFailed)
        {
            Undos.Reset();
            bFinished = true;
        }
    }

    /** Undoes every applied step in reverse order; false when one of them could not be undone. */
    bool Rollback()
    {
        for (int32 Index = Undos.Num() - 1; Index >= 0; --Index)
        {
            if (!Undos[Index].Value())
            {
                bRollbackFailed = true;
                if (FailedUndo.IsEmpty())
                    FailedUndo = Undos[Index].Key;
            }
        }
        Undos.Reset();
        bFinished = true;
        return !bRollbackFailed;
    }

    bool Failed() const { return bFailed; }
    bool RollbackFailed() const { return bRollbackFailed; }
    const FString& GetFailedStep() const { return FailedStep; }
    const FString& GetFailedUndo() const { return FailedUndo; }
    int32 GetUndoCount() const { return Undos.Num(); }

private:
    TArray<TPair<FString, TFunction<bool()>>> Undos;
    FString FailedStep;
    FString FailedUndo;
    bool bFailed = false;
    bool bFinished = false;
    bool bRollbackFailed = false;
};
