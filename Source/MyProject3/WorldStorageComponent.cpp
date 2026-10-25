#include "WorldStorageComponent.h"

#include "Engine/World.h"
#include "WorldObjectIdentityComponent.h"
#include "WorldStorageSubsystem.h"

UWorldStorageComponent::UWorldStorageComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWorldStorageComponent::OnComponentCreated()
{
    Super::OnComponentCreated();

#if WITH_EDITOR
    AActor* Owner = GetOwner();
    if (GIsEditor && IsValid(Owner) &&
        !Owner->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) &&
        !StorageId.IsValid())
    {
        Owner->Modify();
        StorageId = FGuid::NewGuid();
        if (UPackage* Package = Owner->GetOutermost())
            Package->MarkPackageDirty();
    }
#endif
}

void UWorldStorageComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!StorageId.IsValid())
    {
        if (AActor* Owner = GetOwner())
        {
            if (UWorldObjectIdentityComponent* Identity =
                Owner->FindComponentByClass<UWorldObjectIdentityComponent>())
            {
                Identity->EnsureIdentity();
                StorageId = Identity->GetInstanceId();
            }
        }
        if (!StorageId.IsValid())
            StorageId = FGuid::NewGuid();
    }

    UWorldStorageSubsystem* Storage =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    bIsRegistered = Storage && Storage->RegisterStorage(StorageId, CapacityUnits);
    if (!bIsRegistered)
    {
        UE_LOG(LogTemp, Error, TEXT("Storage component on '%s' failed to register (ID=%s, capacity=%d)."),
            *GetNameSafe(GetOwner()), *StorageId.ToString(), CapacityUnits);
    }
}

void UWorldStorageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bIsRegistered)
    {
        UWorldStorageSubsystem* Storage =
            GetWorld() ? GetWorld()->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
        if (Storage && !Storage->SetStorageAvailable(StorageId, false))
        {
            UE_LOG(LogTemp, Error, TEXT("Storage '%s' could not be marked unavailable during EndPlay."),
                *StorageId.ToString());
        }
        bIsRegistered = false;
    }

    Super::EndPlay(EndPlayReason);
}

bool UWorldStorageComponent::CanAcceptItem(FGuid ItemId) const
{
    const UWorldStorageSubsystem* Storage =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    return Storage && Storage->CanAcceptItem(StorageId, ItemId);
}

bool UWorldStorageComponent::GetStoredItemIds(TArray<FGuid>& OutItemIds) const
{
    UWorldStorageSubsystem* Storage =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStorageSubsystem>() : nullptr;
    if (!Storage)
    {
        OutItemIds.Reset();
        return false;
    }
    return Storage->GetStoredItemIds(StorageId, OutItemIds);
}
