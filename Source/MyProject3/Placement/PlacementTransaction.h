#pragma once

#include "CoreMinimal.h"

/**
 * All or nothing: each step is applied with its undo. When a step fails, every
 * step before it is undone in reverse order. A transaction that is neither
 * committed nor rolled back rolls back when it goes out of scope.
 *
 *   FPlacementTransaction Tx;
 *   Tx.Step(TEXT("Graph"), [&] { return Graph->TryConnect(...); }, [&] { Graph->TryDisconnect(...); });
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
    bool Step(const TCHAR* Name, TFunctionRef<bool()> Apply, TFunction<void()> Undo = nullptr)
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
            Undos.Add(MoveTemp(Undo));
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

    void Rollback()
    {
        for (int32 Index = Undos.Num() - 1; Index >= 0; --Index)
            Undos[Index]();
        Undos.Reset();
        bFinished = true;
    }

    bool Failed() const { return bFailed; }
    const FString& GetFailedStep() const { return FailedStep; }
    int32 GetUndoCount() const { return Undos.Num(); }

private:
    TArray<TFunction<void()>> Undos;
    FString FailedStep;
    bool bFailed = false;
    bool bFinished = false;
};
