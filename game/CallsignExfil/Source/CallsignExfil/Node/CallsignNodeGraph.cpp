// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNodeGraph.h"
#include "CallsignNode.h"
#include "Kismet/GameplayStatics.h"

ACallsignNodeGraph::ACallsignNodeGraph()
{
        PrimaryActorTick.bCanEverTick = false;
}

void ACallsignNodeGraph::BeginPlay()
{
        Super::BeginPlay();

        if (Nodes.Num() == 0)
        {
                TArray<AActor*> Found;
                UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACallsignNode::StaticClass(), Found);
                Nodes.Reserve(Found.Num());
                for (AActor* Actor : Found)
                {
                        if (ACallsignNode* Node = Cast<ACallsignNode>(Actor))
                        {
                                Nodes.Add(Node);
                        }
                }
        }
}

ACallsignNode* ACallsignNodeGraph::FindNodeByName(FName Name) const
{
        for (const TObjectPtr<ACallsignNode>& Node : Nodes)
        {
                if (Node && Node->GetFName() == Name)
                {
                        return Node;
                }
        }
        return nullptr;
}

TArray<ACallsignNode*> ACallsignNodeGraph::GetNeighbors(ACallsignNode* Node) const
{
        TArray<ACallsignNode*> Result;
        if (!Node)
        {
                return Result;
        }

        Result.Reserve(Node->Adjacent.Num());
        for (const TObjectPtr<ACallsignNode>& Neighbor : Node->Adjacent)
        {
                if (Neighbor)
                {
                        Result.Add(Neighbor);
                }
        }
        return Result;
}
