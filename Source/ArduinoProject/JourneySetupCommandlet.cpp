#include "JourneySetupCommandlet.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_SwitchString.h"
#include "K2Node_Self.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "MediaPlayer.h"
#include "InputCoreTypes.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "K2Node_FunctionEntry.h"
#include "NodeFactory.h"
#include "SGraphNode.h"
#include "Framework/Application/SlateApplication.h"
#include "JourneyVideoLibrary.h"

namespace JourneySetup
{
    const UEdGraphSchema_K2* Schema() { return GetDefault<UEdGraphSchema_K2>(); }

    template<class T> T* Node(UEdGraph* Graph, int32 X, int32 Y)
    {
        T* Result = NewObject<T>(Graph);
        Graph->AddNode(Result, false, false);
        Result->CreateNewGuid();
        Result->NodePosX = X;
        Result->NodePosY = Y;
        return Result;
    }

    UEdGraphPin* Pin(UEdGraphNode* N, const TCHAR* Name)
    {
        UEdGraphPin* P = N->FindPin(FName(Name));
        checkf(P, TEXT("Missing pin %s on %s"), Name, *N->GetName());
        return P;
    }

    void Link(UEdGraphNode* A, const TCHAR* AP, UEdGraphNode* B, const TCHAR* BP)
    {
        checkf(Schema()->TryCreateConnection(Pin(A, AP), Pin(B, BP)), TEXT("Cannot connect %s.%s to %s.%s"), *A->GetName(), AP, *B->GetName(), BP);
    }

    void Value(UEdGraphNode* N, const TCHAR* P, const TCHAR* V)
    {
        Schema()->TrySetDefaultValue(*Pin(N, P), V);
    }

    UK2Node_CallFunction* Call(UEdGraph* G, UClass* Class, const TCHAR* Name, int32 X, int32 Y)
    {
        auto* N = Node<UK2Node_CallFunction>(G, X, Y);
        N->SetFromFunction(Class->FindFunctionByName(FName(Name)));
        N->AllocateDefaultPins();
        return N;
    }

    UK2Node_CallFunction* SelfCall(UEdGraph* G, const TCHAR* Name, int32 X, int32 Y)
    {
        auto* N = Node<UK2Node_CallFunction>(G, X, Y);
        N->FunctionReference.SetSelfMember(FName(Name));
        N->AllocateDefaultPins();
        return N;
    }

    UK2Node_CustomEvent* Event(UEdGraph* G, const TCHAR* Name, int32 X, int32 Y, bool Command = false)
    {
        auto* N = Node<UK2Node_CustomEvent>(G, X, Y);
        N->CustomFunctionName = FName(Name);
        N->AllocateDefaultPins();
        if (Command)
        {
            FEdGraphPinType Type;
            Type.PinCategory = UEdGraphSchema_K2::PC_String;
            N->CreateUserDefinedPin(TEXT("Command"), Type, EGPD_Output);
        }
        return N;
    }

    UK2Node_VariableGet* Get(UEdGraph* G, const TCHAR* Name, int32 X, int32 Y)
    {
        auto* N = Node<UK2Node_VariableGet>(G, X, Y);
        N->VariableReference.SetSelfMember(FName(Name));
        N->AllocateDefaultPins();
        return N;
    }

    UK2Node_VariableSet* Set(UEdGraph* G, const TCHAR* Name, const TCHAR* V, int32 X, int32 Y)
    {
        auto* N = Node<UK2Node_VariableSet>(G, X, Y);
        N->VariableReference.SetSelfMember(FName(Name));
        N->AllocateDefaultPins();
        if (V) Value(N, Name, V);
        return N;
    }

    UK2Node_IfThenElse* Branch(UEdGraph* G, int32 X, int32 Y)
    {
        auto* N = Node<UK2Node_IfThenElse>(G, X, Y);
        N->AllocateDefaultPins();
        return N;
    }

    UEdGraphNode* Named(UEdGraph* G, const TCHAR* Name)
    {
        for (UEdGraphNode* N : G->Nodes) if (N->GetFName() == FName(Name)) return N;
        checkf(false, TEXT("Missing original node %s"), Name);
        return nullptr;
    }

    void InsertAfter(UEdGraphNode* Existing, const TCHAR* Output, UEdGraphNode* Added)
    {
        UEdGraphPin* P = Pin(Existing, Output);
        TArray<UEdGraphPin*> OldLinks = P->LinkedTo;
        P->BreakAllPinLinks();
        Link(Existing, Output, Added, TEXT("execute"));
        for (UEdGraphPin* Old : OldLinks)
            check(Schema()->TryCreateConnection(Pin(Added, TEXT("then")), Old));
    }

    void Comment(UEdGraph* G, const TCHAR* Text, int32 X, int32 Y, int32 W, int32 H)
    {
        auto* N = Node<UEdGraphNode_Comment>(G, X, Y);
        N->NodeComment = Text;
        N->NodeWidth = W;
        N->NodeHeight = H;
        N->CommentColor = FLinearColor(0.08f, 0.2f, 0.28f, 1.f);
        N->FontSize = 20;
    }

    /** Collect executable descendants and the pure nodes that supply their inputs. */
    void Collect(UEdGraphNode* N, TSet<UEdGraphNode*>& Nodes)
    {
        if (Nodes.Contains(N)) return;
        Nodes.Add(N);
        for (UEdGraphPin* P : N->Pins)
        {
            const bool Exec = P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
            if ((Exec && P->Direction == EGPD_Output) || (!Exec && P->Direction == EGPD_Input))
                for (UEdGraphPin* L : P->LinkedTo) Collect(L->GetOwningNode(), Nodes);
        }
    }

    bool IsRoot(UEdGraphNode* N)
    {
        return N->IsA<UK2Node_Event>() || N->IsA<UK2Node_InputKey>() || N->IsA<UK2Node_FunctionEntry>();
    }

    bool HasExec(UEdGraphNode* N)
    {
        for (UEdGraphPin* P : N->Pins)
            if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) return true;
        return false;
    }

    bool Tidy(UBlueprint* BP, bool GestureControls = false)
    {
        const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const int64 BeforeBytes = IFileManager::Get().FileSize(*Filename);
        const FString Backup = FPaths::ProjectSavedDir() / TEXT("JourneyBackup/BP_ArduinoController.before-idle-reset-and-layout.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup)) IFileManager::Get().Copy(*Backup, *Filename);
        TArray<UEdGraph*> Graphs;
        BP->GetAllGraphs(Graphs);
        // Preserve the installed gesture protocol on later cleanup runs.
        for (UEdGraph* G : Graphs)
            for (UEdGraphNode* N : G->Nodes)
                if (auto* Router = Cast<UK2Node_SwitchString>(N))
                    if (Router->PinNames.Num() == 2) GestureControls = true;
        int32 BeforeNodes = 0;
        for (UEdGraph* G : Graphs) BeforeNodes += G->Nodes.Num();

        // One startup call selects the loose video files for all existing playback paths.
        for (UEdGraph* G : Graphs)
        {
            bool Installed = false;
            UK2Node_Event* Begin = nullptr;
            for (UEdGraphNode* N : G->Nodes)
            {
                if (auto* C = Cast<UK2Node_CallFunction>(N))
                    Installed |= C->FunctionReference.GetMemberName() == TEXT("LoadVideoConfiguration");
                if (auto* E = Cast<UK2Node_Event>(N))
                    if (E->EventReference.GetMemberName() == TEXT("ReceiveBeginPlay")) Begin = E;
            }
            if (!Begin || Installed) continue;
            TArray<UEdGraphPin*> Original = Pin(Begin, TEXT("then"))->LinkedTo;
            Pin(Begin, TEXT("then"))->BreakAllPinLinks();
            auto* Load = Call(G, UJourneyVideoLibrary::StaticClass(), TEXT("LoadVideoConfiguration"), 0, 0);
            auto* Valid = Branch(G, 0, 0);
            auto* Idle = Set(G, TEXT("IdleSource"), nullptr, 0, 0);
            auto* Main = Set(G, TEXT("InteractionSource"), nullptr, 0, 0);
            Link(Begin, TEXT("then"), Load, TEXT("execute"));
            Link(Load, TEXT("then"), Valid, TEXT("execute"));
            Link(Load, TEXT("ReturnValue"), Valid, TEXT("Condition"));
            Link(Valid, TEXT("then"), Idle, TEXT("execute"));
            Link(Load, TEXT("IdleVideo"), Idle, TEXT("IdleSource"));
            Link(Idle, TEXT("then"), Main, TEXT("execute"));
            Link(Load, TEXT("MainVideo"), Main, TEXT("InteractionSource"));
            for (UEdGraphPin* Next : Original)
            {
                check(Schema()->TryCreateConnection(Pin(Main, TEXT("then")), Next));
                check(Schema()->TryCreateConnection(Pin(Valid, TEXT("else")), Next));
            }
        }

        if (GestureControls)
        {
            for (UEdGraph* G : Graphs)
            {
                UK2Node_SwitchString* Router = nullptr;
                for (UEdGraphNode* N : G->Nodes)
                    if (auto* Switch = Cast<UK2Node_SwitchString>(N)) Router = Switch;
                if (!Router || Router->PinNames.Num() != 3) continue;
                // Replace the old action branches; the shared event and serial input stay intact.
                for (const TCHAR* Case : {TEXT("0"), TEXT("1"), TEXT("2")})
                    Pin(Router, Case)->BreakAllPinLinks();
                Router->PinNames = {FName(TEXT("0")), FName(TEXT("1"))};
                Router->ReconstructNode();
                auto* Active = Get(G, TEXT("IsInteractionActive"), 0, 0);
                auto* IsJourney = Branch(G, 0, 0);
                auto* Start = SelfCall(G, TEXT("StartInteraction"), 0, 0);
                auto* Toggle = SelfCall(G, TEXT("ToggleInteractionPause"), 0, 0);
                auto* Refresh = SelfCall(G, TEXT("RefreshInactivityTimer"), 0, 0);
                auto* Reset = SelfCall(G, TEXT("PlayIdle"), 0, 0);
                Link(Router, TEXT("0"), IsJourney, TEXT("execute"));
                Link(Active, TEXT("IsInteractionActive"), IsJourney, TEXT("Condition"));
                Link(IsJourney, TEXT("then"), Toggle, TEXT("execute"));
                Link(IsJourney, TEXT("else"), Start, TEXT("execute"));
                Link(Start, TEXT("then"), Refresh, TEXT("execute"));
                Link(Toggle, TEXT("then"), Refresh, TEXT("execute"));
                Link(Router, TEXT("1"), Reset, TEXT("execute"));
                // K/T/P all simulate a single tap; R simulates one recognized double tap.
                for (UEdGraphNode* N : G->Nodes)
                    if (auto* Key = Cast<UK2Node_InputKey>(N))
                        for (UEdGraphPin* Target : Pin(Key, TEXT("Pressed"))->LinkedTo)
                            if (auto* CallNode = Cast<UK2Node_CallFunction>(Target->GetOwningNode()))
                                if (CallNode->FunctionReference.GetMemberName() == TEXT("HandleArduinoCommand"))
                                    Value(CallNode, TEXT("Command"), Key->InputKey == EKeys::R ? TEXT("1") : TEXT("0"));
            }
        }

        // Command 2 uses the existing idle path. Delete the redundant restart event,
        // state setters, and timer refresh, rather than adding another reset path.
        for (UEdGraph* G : Graphs)
        {
            for (UEdGraphNode* N : TArray<UEdGraphNode*>(G->Nodes))
            {
                if (auto* C = Cast<UK2Node_CallFunction>(N))
                {
                    if (C->FunctionReference.GetMemberName() == TEXT("RestartInteraction"))
                    {
                        TArray<UEdGraphPin*> Next = Pin(C, TEXT("then"))->LinkedTo;
                        Pin(C, TEXT("then"))->BreakAllPinLinks();
                        for (UEdGraphPin* P : Next)
                            if (auto* Refresh = Cast<UK2Node_CallFunction>(P->GetOwningNode()))
                                if (Refresh->FunctionReference.GetMemberName() == TEXT("RefreshInactivityTimer"))
                                    FBlueprintEditorUtils::RemoveNode(BP, Refresh, true);
                        C->FunctionReference.SetSelfMember(TEXT("PlayIdle"));
                        C->ReconstructNode();
                    }
                }
                if (auto* E = Cast<UK2Node_CustomEvent>(N))
                    if (E->CustomFunctionName == TEXT("RestartInteraction"))
                        FBlueprintEditorUtils::RemoveNode(BP, E, true);
            }
        }

        TMap<UEdGraphNode*, FVector2D> Sizes;
        int32 RemovedUnused = 0;
        FString LayoutRows;
        int32 Overlaps = 0;
        for (UEdGraph* G : Graphs)
        {
            TSet<UEdGraphNode*> Live;
            for (UEdGraphNode* N : G->Nodes) if (IsRoot(N)) Collect(N, Live);
            for (UEdGraphNode* N : TArray<UEdGraphNode*>(G->Nodes))
            {
                if (N->IsA<UEdGraphNode_Comment>() || !Live.Contains(N))
                {
                    if (!N->IsA<UEdGraphNode_Comment>()) ++RemovedUnused;
                    FBlueprintEditorUtils::RemoveNode(BP, N, true);
                }
            }

            TArray<TArray<UEdGraphNode*>> RootGroups;
            TArray<UEdGraphNode*> Keys;
            for (UEdGraphNode* N : G->Nodes)
                if (N->IsA<UK2Node_InputKey>()) Keys.Add(N);
            if (!Keys.IsEmpty()) RootGroups.Add(Keys);
            for (UEdGraphNode* N : G->Nodes)
                if (IsRoot(N) && !N->IsA<UK2Node_InputKey>()) RootGroups.Add({N});

            TSet<UEdGraphNode*> Placed;
            int32 GroupY = 100;
            for (const TArray<UEdGraphNode*>& Roots : RootGroups)
            {
                TSet<UEdGraphNode*> Group;
                for (UEdGraphNode* Root : Roots) Collect(Root, Group);
                for (UEdGraphNode* Old : Placed) Group.Remove(Old);
                if (Group.IsEmpty()) continue;

                // Share identical variable getters within a section, keeping wires local.
                TMap<FName, UK2Node_VariableGet*> Getters;
                for (UEdGraphNode* N : TArray<UEdGraphNode*>(G->Nodes))
                {
                    auto* Getter = Cast<UK2Node_VariableGet>(N);
                    if (!Getter || !Group.Contains(N) || !Getter->VariableReference.IsSelfContext()) continue;
                    FName Name = Getter->VariableReference.GetMemberName();
                    if (UK2Node_VariableGet** Existing = Getters.Find(Name))
                    {
                        UEdGraphPin* Out = Getter->FindPin(Name, EGPD_Output);
                        UEdGraphPin* Shared = (*Existing)->FindPin(Name, EGPD_Output);
                        if (!Out || !Shared) continue;
                        TArray<UEdGraphPin*> Links = Out->LinkedTo;
                        Out->BreakAllPinLinks();
                        for (UEdGraphPin* L : Links) check(Schema()->TryCreateConnection(Shared, L));
                        Group.Remove(N);
                        FBlueprintEditorUtils::RemoveNode(BP, N, true);
                        ++RemovedUnused;
                    }
                    else Getters.Add(Name, Getter);
                }

                // Longest execution-path depth keeps branches and subsequent calls ordered.
                TMap<UEdGraphNode*, int32> Depth;
                for (UEdGraphNode* Root : Roots) Depth.Add(Root, 0);
                for (int32 Iter = 0; Iter < Group.Num(); ++Iter)
                {
                    bool Changed = false;
                    for (UEdGraphNode* N : G->Nodes)
                    {
                        if (!Group.Contains(N) || !Depth.Contains(N)) continue;
                        for (UEdGraphPin* P : N->Pins)
                            if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                                for (UEdGraphPin* L : P->LinkedTo)
                                {
                                    UEdGraphNode* Next = L->GetOwningNode();
                                    if (Group.Contains(Next) && (!Depth.Contains(Next) || Depth[Next] < Depth[N] + 1))
                                    { Depth.Add(Next, Depth[N] + 1); Changed = true; }
                                }
                    }
                    if (!Changed) break;
                }

                // Pure dependencies sit below execution and one column before consumers.
                for (int32 Iter = 0; Iter < Group.Num(); ++Iter)
                    for (UEdGraphNode* N : G->Nodes)
                        if (Group.Contains(N) && !HasExec(N) && !Depth.Contains(N))
                            for (UEdGraphPin* P : N->Pins)
                                if (P->Direction == EGPD_Output)
                                    for (UEdGraphPin* L : P->LinkedTo)
                                        if (Depth.Contains(L->GetOwningNode()))
                                            Depth.Add(N, FMath::Max(0, Depth[L->GetOwningNode()] - 1));

                int32 MaxDepth = 0;
                TMap<int32, double> Widths;
                for (UEdGraphNode* N : G->Nodes)
                {
                    if (!Group.Contains(N)) continue;
                    const int32 Column = Depth.FindRef(N);
                    MaxDepth = FMath::Max(MaxDepth, Column);
                    FVector2D Size(420, 130 + N->Pins.Num() * 22);
                    if (FSlateApplication::IsInitialized())
                    {
                        TSharedPtr<SGraphNode> Widget = FNodeFactory::CreateNodeWidget(N);
                        if (Widget.IsValid())
                        {
                            Widget->SlatePrepass(1.f);
                            Size = FVector2D(Widget->GetDesiredSize());
                        }
                    }
                    Size.X = FMath::Max(Size.X, 120.0);
                    Size.Y = FMath::Max(Size.Y, 48.0);
                    Sizes.Add(N, Size);
                    Widths.FindOrAdd(Column) = FMath::Max(Widths.FindRef(Column), Size.X);
                }

                TMap<int32, int32> ColumnX;
                int32 X = 100;
                for (int32 Column = 0; Column <= MaxDepth; ++Column)
                { ColumnX.Add(Column, X); X += FMath::CeilToInt(Widths.FindRef(Column)) + 160; }
                int32 ExecBottom = GroupY;
                TMap<int32, int32> ColumnY;
                for (UEdGraphNode* N : G->Nodes)
                {
                    if (!Group.Contains(N) || !HasExec(N)) continue;
                    int32 Column = Depth.FindRef(N);
                    int32 Y = ColumnY.Contains(Column) ? ColumnY[Column] : GroupY;
                    N->NodePosX = ColumnX.FindRef(Column);
                    N->NodePosY = Y;
                    ColumnY.Add(Column, Y + FMath::CeilToInt(Sizes[N].Y) + 100);
                    ExecBottom = FMath::Max(ExecBottom, ColumnY[Column]);
                }
                ColumnY.Empty();
                int32 Bottom = ExecBottom;
                for (UEdGraphNode* N : G->Nodes)
                {
                    if (!Group.Contains(N) || HasExec(N)) continue;
                    int32 Column = Depth.FindRef(N);
                    int32 Y = ColumnY.Contains(Column) ? ColumnY[Column] : ExecBottom + 60;
                    N->NodePosX = ColumnX.FindRef(Column);
                    N->NodePosY = Y;
                    ColumnY.Add(Column, Y + FMath::CeilToInt(Sizes[N].Y) + 70);
                    Bottom = FMath::Max(Bottom, ColumnY[Column]);
                }
                FString Title = Roots.Num() > 1 ? (GestureControls ? TEXT("Keyboard test | K/T/P single tap, R double tap reset") : TEXT("Keyboard test | K/T start, P pause/resume, R reset to idle")) : Roots[0]->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "));
                if (auto* E = Cast<UK2Node_CustomEvent>(Roots[0]))
                {
                    if (E->CustomFunctionName == TEXT("HandleArduinoCommand")) Title = GestureControls ? TEXT("Arduino gestures | 0 single tap: start or pause/resume; 1 double tap: idle") : TEXT("Arduino commands | 0 start, 1 pause/resume, 2 reset to idle");
                    if (E->CustomFunctionName == TEXT("RefreshInactivityTimer")) Title = TEXT("Inactivity | Editable minutes; timeout resets to idle");
                }
                if (Group.Num() > 1)
                    Comment(G, *Title, 40, GroupY - 80, FMath::Max(X - 40, Title.Len() * 14 + 80), Bottom - GroupY + 140);
                Placed.Append(Group);
                GroupY = Bottom + 280;
            }

            // Validate actual node rectangles; comment boxes intentionally surround nodes.
            TArray<UEdGraphNode*> Body;
            for (UEdGraphNode* N : G->Nodes) if (!N->IsA<UEdGraphNode_Comment>()) Body.Add(N);
            for (int32 A = 0; A < Body.Num(); ++A)
                for (int32 B = A + 1; B < Body.Num(); ++B)
                {
                    UEdGraphNode* NA = Body[A]; UEdGraphNode* NB = Body[B];
                    const FVector2D SA = Sizes.FindRef(NA), SB = Sizes.FindRef(NB);
                    if (NA->NodePosX < NB->NodePosX + SB.X && NB->NodePosX < NA->NodePosX + SA.X &&
                        NA->NodePosY < NB->NodePosY + SB.Y && NB->NodePosY < NA->NodePosY + SA.Y) ++Overlaps;
                }
            LayoutRows += FString::Printf(TEXT("%s: %d functional nodes, no disconnected nodes\n"), *G->GetName(), Body.Num());
        }
        if (Overlaps) { UE_LOG(LogTemp, Error, TEXT("Layout contains %d overlaps; not saving."), Overlaps); return false; }
        FCompilerResultsLog Results;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
        if (Results.NumErrors || BP->Status == BS_Error) return false;
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, Args)) return false;
        int32 AfterNodes = 0;
        for (UEdGraph* G : Graphs) AfterNodes += G->Nodes.Num();
        const FString Report = FString::Printf(TEXT("Slate measurement: %s\nOverlapping node pairs: %d\nNodes before: %d\nNodes after: %d\nUnused/duplicate nodes removed: %d\nAsset bytes before: %lld\nAsset bytes after: %lld\n%s"), FSlateApplication::IsInitialized() ? TEXT("yes") : TEXT("no (conservative sizing)"), Overlaps, BeforeNodes, AfterNodes, RemovedUnused, BeforeBytes, IFileManager::Get().FileSize(*Filename), *LayoutRows);
        FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("JourneyLayoutVerification.txt")));
        UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
        return true;
    }

    bool Apply(UBlueprint* BP)
    {
        // Blueprint's MediaOpened event owns playback; auto-play runs after that
        // event and would otherwise override a pause requested during opening.
        UMediaPlayer* MediaAsset = LoadObject<UMediaPlayer>(nullptr, TEXT("/Game/Movies/MP_Experience.MP_Experience"));
        check(MediaAsset);
        if (MediaAsset->PlayOnOpen)
        {
            const FString MediaFilename = FPackageName::LongPackageNameToFilename(MediaAsset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
            const FString Backup = FPaths::ProjectSavedDir() / TEXT("JourneyBackup/MP_Experience.before-command-update.uasset");
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
            if (!IFileManager::Get().FileExists(*Backup)) IFileManager::Get().Copy(*Backup, *MediaFilename);
            MediaAsset->Modify();
            MediaAsset->PlayOnOpen = false;
            MediaAsset->MarkPackageDirty();
            FSavePackageArgs Args;
            Args.TopLevelFlags = RF_Public | RF_Standalone;
            if (!UPackage::SavePackage(MediaAsset->GetOutermost(), MediaAsset, *MediaFilename, Args)) return false;
        }
        if (BP->SkeletonGeneratedClass->FindFunctionByName(TEXT("HandleArduinoCommand")))
        {
            UE_LOG(LogTemp, Display, TEXT("Journey command graph already installed; no changes."));
            return true;
        }
        UEdGraph* Touch = nullptr;
        UEdGraph* Playback = nullptr;
        for (UEdGraph* G : BP->UbergraphPages)
        {
            if (G->GetName() == TEXT("Touch Input")) Touch = G;
            if (G->GetName() == TEXT("Video Playback")) Playback = G;
        }
        check(Touch && Playback);

        FEdGraphPinType TimeoutType;
        TimeoutType.PinCategory = UEdGraphSchema_K2::PC_Real;
        TimeoutType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        check(FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("InactivityTimeoutMinutes"), TimeoutType, TEXT("5.0")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, TEXT("InactivityTimeoutMinutes"), nullptr, FText::FromString(TEXT("Journey | Settings")));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, TEXT("InactivityTimeoutMinutes"), nullptr, TEXT("DisplayName"), TEXT("Inactivity Timeout (Minutes)"));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, TEXT("InactivityTimeoutMinutes"), nullptr, TEXT("ClampMin"), TEXT("0.001"));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, TEXT("InactivityTimeoutMinutes"), nullptr, TEXT("ToolTip"), TEXT("Minutes since the last valid Arduino or test-key command before returning to idle. Default 5. Applies while playing, paused, or opening."));
        for (FBPVariableDescription& V : BP->NewVariables)
            if (V.VarName == TEXT("InactivityTimeoutMinutes")) V.PropertyFlags &= ~CPF_DisableEditOnInstance;
        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        check(FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("WantsPaused"), BoolType, TEXT("false")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, TEXT("WantsPaused"), nullptr, FText::FromString(TEXT("Playback | State")));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);

        // Declare the new events before generating calls to their skeleton functions.
        auto* Dispatch = Event(Touch, TEXT("HandleArduinoCommand"), 0, 1500, true);
        auto* Refresh = Event(Touch, TEXT("RefreshInactivityTimer"), 0, 2700);
        auto* Toggle = Event(Touch, TEXT("ToggleInteractionPause"), 0, 3400);
        auto* Restart = Event(Touch, TEXT("RestartInteraction"), 0, 4500);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);

        // Serial and keyboard use the same command router. Unknown messages do nothing.
        UEdGraphNode* SerialEvent = Named(Touch, TEXT("K2Node_ComponentBoundEvent_0"));
        UEdGraphNode* Trim = Named(Touch, TEXT("K2Node_CallFunction_1"));
        Pin(SerialEvent, TEXT("then"))->BreakAllPinLinks();
        Pin(Trim, TEXT("ReturnValue"))->BreakAllPinLinks();
        for (const TCHAR* Name : { TEXT("K2Node_CallFunction_2"), TEXT("K2Node_IfThenElse_0"), TEXT("K2Node_CallFunction_3"), TEXT("K2Node_CallFunction_0") })
            FBlueprintEditorUtils::RemoveNode(BP, Named(Touch, Name), true);
        auto* SerialRoute = SelfCall(Touch, TEXT("HandleArduinoCommand"), SerialEvent->NodePosX + 600, SerialEvent->NodePosY);
        Link(SerialEvent, TEXT("then"), SerialRoute, TEXT("execute"));
        Link(Trim, TEXT("ReturnValue"), SerialRoute, TEXT("Command"));
        auto* OriginalT = CastChecked<UK2Node_InputKey>(Named(Touch, TEXT("K2Node_InputKey_0")));
        auto AddKeyboard = [&](FKey Key, const TCHAR* Command, int32 X, UK2Node_InputKey* Existing = nullptr)
        {
            auto* Input = Existing ? Existing : Node<UK2Node_InputKey>(Touch, X, 0);
            if (!Existing) { Input->InputKey = Key; Input->AllocateDefaultPins(); }
            auto* Route = SelfCall(Touch, TEXT("HandleArduinoCommand"), X + 180, 0);
            Value(Route, TEXT("Command"), Command);
            Link(Input, TEXT("Pressed"), Route, TEXT("execute"));
        };
        AddKeyboard(EKeys::T, TEXT("0"), OriginalT->NodePosX, OriginalT);
        AddKeyboard(EKeys::K, TEXT("0"), 1000);
        AddKeyboard(EKeys::P, TEXT("1"), 1650);
        AddKeyboard(EKeys::R, TEXT("2"), 2300);
        Named(Touch, TEXT("EdGraphNode_Comment_0"))->NodeComment = TEXT("KEYBOARD TEST | T or K = start; P = pause/resume; R = restart main video");
        Named(Touch, TEXT("EdGraphNode_Comment_1"))->NodeComment = TEXT("ARDUINO | 0 = start, 1 = double tap pause/resume, 2 = five-second hold restart");

        auto* Switch = Node<UK2Node_SwitchString>(Touch, 300, 1500);
        Switch->PinNames = { FName(TEXT("0")), FName(TEXT("1")), FName(TEXT("2")) };
        Switch->AllocateDefaultPins();
        Link(Dispatch, TEXT("then"), Switch, TEXT("execute"));
        Link(Dispatch, TEXT("Command"), Switch, TEXT("Selection"));
        int32 Row = 0;
        for (const TCHAR* Function : { TEXT("StartInteraction"), TEXT("ToggleInteractionPause"), TEXT("RestartInteraction") })
        {
            const FString Command = FString::FromInt(Row);
            auto* Action = SelfCall(Touch, Function, 650, 1500 + Row * 260);
            auto* Activity = SelfCall(Touch, TEXT("RefreshInactivityTimer"), 1050, 1500 + Row * 260);
            Link(Switch, *Command, Action, TEXT("execute"));
            Link(Action, TEXT("then"), Activity, TEXT("execute"));
            ++Row;
        }
        Comment(Touch, TEXT("COMMAND ROUTER | Every valid command refreshes the inactivity timer; unknown messages are ignored"), -50, 1400, 1550, 950);

        auto* Active = Get(Touch, TEXT("IsInteractionActive"), 0, 2900);
        auto* ActiveBranch = Branch(Touch, 320, 2700);
        Link(Refresh, TEXT("then"), ActiveBranch, TEXT("execute"));
        Link(Active, TEXT("IsInteractionActive"), ActiveBranch, TEXT("Condition"));
        auto* Timer = Call(Touch, UKismetSystemLibrary::StaticClass(), TEXT("K2_SetTimer"), 950, 2700);
        Value(Timer, TEXT("FunctionName"), TEXT("PlayIdle"));
        Value(Timer, TEXT("bLooping"), TEXT("false"));
        auto* Self = Node<UK2Node_Self>(Touch, 650, 2950);
        Self->AllocateDefaultPins();
        Link(Self, TEXT("self"), Timer, TEXT("Object"));
        Link(ActiveBranch, TEXT("then"), Timer, TEXT("execute"));
        auto* Minutes = Get(Touch, TEXT("InactivityTimeoutMinutes"), 300, 3100);
        auto* Seconds = Call(Touch, UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"), 650, 3100);
        Value(Seconds, TEXT("B"), TEXT("60.0"));
        Link(Minutes, TEXT("InactivityTimeoutMinutes"), Seconds, TEXT("A"));
        Link(Seconds, TEXT("ReturnValue"), Timer, TEXT("Time"));
        Comment(Touch, TEXT("INACTIVITY | Editable minutes x 60; single timer resets after each valid command; expires to idle"), -50, 2600, 1600, 700);

        // Store desired pause state even if a command arrives while media is still opening.
        auto* ToggleActive = Get(Touch, TEXT("IsInteractionActive"), 0, 3600);
        auto* ToggleBranch = Branch(Touch, 300, 3400);
        Link(Toggle, TEXT("then"), ToggleBranch, TEXT("execute"));
        Link(ToggleActive, TEXT("IsInteractionActive"), ToggleBranch, TEXT("Condition"));
        auto* Desired = Get(Touch, TEXT("WantsPaused"), 300, 3700);
        auto* Not = Call(Touch, UKismetMathLibrary::StaticClass(), TEXT("Not_PreBool"), 550, 3700);
        Link(Desired, TEXT("WantsPaused"), Not, TEXT("A"));
        auto* SetDesired = Set(Touch, TEXT("WantsPaused"), nullptr, 800, 3400);
        Link(Not, TEXT("ReturnValue"), SetDesired, TEXT("WantsPaused"));
        Link(ToggleBranch, TEXT("then"), SetDesired, TEXT("execute"));
        auto* PauseBranch = Branch(Touch, 1100, 3400);
        Link(SetDesired, TEXT("then"), PauseBranch, TEXT("execute"));
        Link(SetDesired, TEXT("Output_Get"), PauseBranch, TEXT("Condition"));
        auto* Player = Get(Touch, TEXT("ExperiencePlayer"), 1100, 3750);
        auto* Pause = Call(Touch, UMediaPlayer::StaticClass(), TEXT("Pause"), 1400, 3400);
        auto* Resume = Call(Touch, UMediaPlayer::StaticClass(), TEXT("Play"), 1400, 3650);
        Link(PauseBranch, TEXT("then"), Pause, TEXT("execute"));
        Link(PauseBranch, TEXT("else"), Resume, TEXT("execute"));
        Link(Player, TEXT("ExperiencePlayer"), Pause, TEXT("self"));
        Link(Player, TEXT("ExperiencePlayer"), Resume, TEXT("self"));
        Comment(Touch, TEXT("DOUBLE TAP | Toggle main-video pause/resume; remember pause commands received during opening; idle is unaffected"), -50, 3300, 1800, 750);

        // Reopen main source rather than rewinding: also works when paused, idle, or opening.
        auto* ResetPause = Set(Touch, TEXT("WantsPaused"), TEXT("false"), 350, 4500);
        auto* ResetActive = Set(Touch, TEXT("IsInteractionActive"), TEXT("false"), 650, 4500);
        auto* Start = SelfCall(Touch, TEXT("StartInteraction"), 1000, 4500);
        Link(Restart, TEXT("then"), ResetPause, TEXT("execute"));
        Link(ResetPause, TEXT("then"), ResetActive, TEXT("execute"));
        Link(ResetActive, TEXT("then"), Start, TEXT("execute"));
        Comment(Touch, TEXT("LONG TOUCH | Arduino detects the five-second hold and sends 2 once; immediately reopen main from the beginning"), -50, 4400, 1500, 430);

        // Clear the timer and pause state whenever the journey returns to idle.
        auto* Clear = Call(Playback, UKismetSystemLibrary::StaticClass(), TEXT("K2_ClearTimer"), -500, 0);
        Value(Clear, TEXT("FunctionName"), TEXT("PlayIdle"));
        auto* PlaybackSelf = Node<UK2Node_Self>(Playback, -750, 250);
        PlaybackSelf->AllocateDefaultPins();
        Link(PlaybackSelf, TEXT("self"), Clear, TEXT("Object"));
        InsertAfter(Named(Playback, TEXT("K2Node_CustomEvent_0")), TEXT("then"), Clear);
        auto* IdlePause = Set(Playback, TEXT("WantsPaused"), TEXT("false"), -200, 0);
        InsertAfter(Clear, TEXT("then"), IdlePause);
        // A fresh start is always playing. Existing IsInteractionActive guard remains.
        auto* StartPause = Set(Playback, TEXT("WantsPaused"), TEXT("false"), 900, 1200);
        InsertAfter(Named(Playback, TEXT("K2Node_VariableSet_1")), TEXT("then"), StartPause);

        // OnMediaOpened applies desired state after the asynchronous open completes.
        auto* Loop = Named(Playback, TEXT("K2Node_CallFunction_5"));
        auto* OpenPlay = Named(Playback, TEXT("K2Node_CallFunction_7"));
        Pin(Loop, TEXT("then"))->BreakAllPinLinks();
        auto* OpenBranch = Branch(Playback, OpenPlay->NodePosX - 250, OpenPlay->NodePosY);
        auto* OpenDesired = Get(Playback, TEXT("WantsPaused"), OpenPlay->NodePosX - 450, OpenPlay->NodePosY + 200);
        auto* OpenPause = Call(Playback, UMediaPlayer::StaticClass(), TEXT("Pause"), OpenPlay->NodePosX, OpenPlay->NodePosY - 250);
        auto* OpenPlayer = Get(Playback, TEXT("ExperiencePlayer"), OpenPlay->NodePosX - 250, OpenPlay->NodePosY - 200);
        Link(Loop, TEXT("then"), OpenBranch, TEXT("execute"));
        Link(OpenDesired, TEXT("WantsPaused"), OpenBranch, TEXT("Condition"));
        Link(OpenBranch, TEXT("then"), OpenPause, TEXT("execute"));
        Link(OpenBranch, TEXT("else"), OpenPlay, TEXT("execute"));
        Link(OpenPlayer, TEXT("ExperiencePlayer"), OpenPause, TEXT("self"));
        Named(Playback, TEXT("EdGraphNode_Comment_2"))->NodeComment = TEXT("MEDIA READY | Idle loops; main respects pause/resume requested while opening");

        FCompilerResultsLog Results;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
        if (Results.NumErrors > 0 || BP->Status == BS_Error)
        {
            UE_LOG(LogTemp, Error, TEXT("Journey Blueprint compilation failed; asset was not saved."));
            return false;
        }
        const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectSavedDir() / TEXT("JourneyBackup/BP_ArduinoController.before-command-update.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup)) IFileManager::Get().Copy(*Backup, *Filename);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        const bool Saved = UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, SaveArgs);
        UE_LOG(LogTemp, Display, TEXT("Journey command graph compiled and saved: %s"), Saved ? TEXT("yes") : TEXT("no"));
        return Saved;
    }
}
#endif

UJourneySetupCommandlet::UJourneySetupCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UJourneySetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Arduino/BP_ArduinoController.BP_ArduinoController"));
    if (!BP) return 1;
    if (Params.Contains(TEXT("-Apply")) && !JourneySetup::Apply(BP)) return 1;
    if ((Params.Contains(TEXT("-TidyReset")) || Params.Contains(TEXT("-Apply"))) && !JourneySetup::Tidy(BP)) return 1;
    FString Report;
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        Report += FString::Printf(TEXT("\nGRAPH %s\n"), *Graph->GetName());
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            Report += FString::Printf(TEXT("NODE %s [%s] %s\n"), *Node->GetName(), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            for (UEdGraphPin* Pin : Node->Pins)
            {
                Report += FString::Printf(TEXT("  %s %s = %s %s"), Pin->Direction == EGPD_Input ? TEXT("IN") : TEXT("OUT"), *Pin->PinName.ToString(), *Pin->DefaultValue, Pin->DefaultObject ? *Pin->DefaultObject->GetPathName() : TEXT(""));
                for (UEdGraphPin* Link : Pin->LinkedTo)
                    Report += FString::Printf(TEXT(" -> %s.%s"), *Link->GetOwningNode()->GetName(), *Link->PinName.ToString());
                Report += TEXT("\n");
            }
        }
    }
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("JourneyBlueprintGraph.txt")));
    UE_LOG(LogTemp, Display, TEXT("Journey graph inspection complete."));
    return 0;
#else
    return 1;
#endif
}

bool UJourneySetupCommandlet::TidyJourneyBlueprint(bool GestureControls)
{
#if WITH_EDITOR
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Arduino/BP_ArduinoController.BP_ArduinoController"));
    return BP && JourneySetup::Tidy(BP, GestureControls);
#else
    return false;
#endif
}
