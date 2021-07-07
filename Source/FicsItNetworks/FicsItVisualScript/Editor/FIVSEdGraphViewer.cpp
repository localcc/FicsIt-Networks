﻿#include "FIVSEdGraphViewer.h"
#include "FicsItNetworks/Reflection/FINReflection.h"

void FFIVSEdConnectionDrawer::Reset() {
	ConnectionUnderMouse = TPair<TSharedPtr<SFIVSEdPinViewer>, TSharedPtr<SFIVSEdPinViewer>>(nullptr, nullptr);
	LastConnectionDistance = FLT_MAX;
}

void FFIVSEdConnectionDrawer::DrawConnection(TSharedRef<SFIVSEdPinViewer> Pin1, TSharedRef<SFIVSEdPinViewer> Pin2, TSharedRef<const SFIVSEdGraphViewer> Graph, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) {
	bool is1Wild = Cast<UFIVSWildcardPin>(Pin1->GetPin());
	bool is2Wild = Cast<UFIVSWildcardPin>(Pin2->GetPin());

	bool bShouldSwitch = false;
	if (is1Wild) {
		if (is2Wild) {
			bool bHasInput = false;
			for (UFIVSPin* Con : Pin1->GetPin()->GetConnections()) if (Con->GetPinType() & FIVS_PIN_INPUT) {
				bHasInput = true;
				break;
			}
			if (Pin1->GetPin()->ParentNode->Pos.X > Pin2->GetPin()->ParentNode->Pos.X && bHasInput) {
				bShouldSwitch = true;
			}
		} else if (!(Pin2->GetPin()->GetPinType() & FIVS_PIN_INPUT)) {
			bShouldSwitch = true;
		}
	} else if (is2Wild) {
		if (!(Pin1->GetPin()->GetPinType() & FIVS_PIN_OUTPUT)) {
			bShouldSwitch = true;
		}
	} else if (!(Pin1->GetPin()->GetPinType() & FIVS_PIN_OUTPUT)) {
		bShouldSwitch = true;
	}
	if (bShouldSwitch) {
		TSharedRef<SFIVSEdPinViewer> Pin = Pin1;
		Pin1 = Pin2;
		Pin2 = Pin;
	}
	FVector2D StartLoc = Graph->GetCachedGeometry().AbsoluteToLocal(Pin1->GetConnectionPoint());
	FVector2D EndLoc = Graph->GetCachedGeometry().AbsoluteToLocal(Pin2->GetConnectionPoint());
	DrawConnection(StartLoc, EndLoc, Pin1->GetPinColor().GetSpecifiedColor(), Graph, AllottedGeometry, OutDrawElements, LayerId);

	// Find the closest approach to the spline
	FVector2D ClosestPoint;
	float ClosestDistanceSquared = FLT_MAX;

	const int32 NumStepsToTest = 16;
	const float StepInterval = 1.0f / (float)NumStepsToTest;
	FVector2D Point1 = FMath::CubicInterp(StartLoc, FVector2D(300, 0), EndLoc, FVector2D(300, 0), 0.0f);
	for (float t = 0.0f; t < 1.0f; t += StepInterval) {
		const FVector2D Point2 = FMath::CubicInterp(StartLoc, FVector2D(300, 0), EndLoc, FVector2D(300, 0), t + StepInterval);

		const FVector2D ClosestPointToSegment = FMath::ClosestPointOnSegment2D(Graph->GetCachedGeometry().AbsoluteToLocal(LastMousePosition), Point1, Point2);
		const float DistanceSquared = (Graph->GetCachedGeometry().AbsoluteToLocal(LastMousePosition) - ClosestPointToSegment).SizeSquared();

		if (DistanceSquared < ClosestDistanceSquared) {
			ClosestDistanceSquared = DistanceSquared;
			ClosestPoint = ClosestPointToSegment;
		}

		Point1 = Point2;
	}
	if (ClosestDistanceSquared < LastConnectionDistance && ClosestDistanceSquared < 100) {
		ConnectionUnderMouse = TPair<TSharedPtr<SFIVSEdPinViewer>, TSharedPtr<SFIVSEdPinViewer>>(Pin1, Pin2);
		LastConnectionDistance = ClosestDistanceSquared;
	}
}

void FFIVSEdConnectionDrawer::DrawConnection(const FVector2D& Start, const FVector2D& End, const FLinearColor& ConnectionColor, TSharedRef<const SFIVSEdGraphViewer> Graph, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) {
	FSlateDrawElement::MakeSpline(OutDrawElements, LayerId+100, AllottedGeometry.ToPaintGeometry(), Start, FVector2D(300 * Graph->Zoom,0), End, FVector2D(300 * Graph->Zoom,0), 2 * Graph->Zoom, ESlateDrawEffect::None, ConnectionColor);
}

void SFIVSEdGraphViewer::Construct(const FArguments& InArgs) {
	SetGraph(InArgs._Graph);
}

SFIVSEdGraphViewer::SFIVSEdGraphViewer() : Children(this) {
	ConnectionDrawer = MakeShared<FFIVSEdConnectionDrawer>();
}

SFIVSEdGraphViewer::~SFIVSEdGraphViewer() {
	if (Graph) Graph->OnNodeChanged.Remove(OnNodeChangedHandle);
}

FVector2D SFIVSEdGraphViewer::ComputeDesiredSize(float) const {
	return FVector2D(1000,1000);
}

int32 SFIVSEdGraphViewer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const {
	ConnectionDrawer->Reset();
	int ret = SPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId+2, InWidgetStyle, bParentEnabled);

	// Draw Grid
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &BackgroundBrush, ESlateDrawEffect::None, BackgroundBrush.TintColor.GetSpecifiedColor());
	
	FVector2D Distance = FVector2D(10,10);
	FVector2D Start = Offset / Distance;
	FVector2D RenderOffset = FVector2D(FMath::Fractional(Start.X) * Distance.X, FMath::Fractional(Start.Y) * Distance.Y);
	int GridOffsetX = FMath::FloorToInt(Start.X) * FMath::RoundToInt(Distance.X);
	int GridOffsetY = FMath::FloorToInt(Start.Y) * FMath::RoundToInt(Distance.Y);
	Distance *= Zoom;
	Start *= Zoom;
	RenderOffset *= Zoom;
	for (float x = 0; x <= AllottedGeometry.GetLocalSize().X; x += Distance.X) {
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), {FVector2D(x + RenderOffset.X, 0), FVector2D(x + RenderOffset.X, AllottedGeometry.GetLocalSize().Y)}, ESlateDrawEffect::None, GridColor, true, ((FMath::RoundToInt(x/Zoom) - GridOffsetX) % 100 == 0) ? 1.0 : 0.05);
	}
	for (float y = 0; y <= AllottedGeometry.GetLocalSize().Y; y += Distance.Y) {
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), {FVector2D(0, y + RenderOffset.Y), FVector2D(AllottedGeometry.GetLocalSize().X, y + RenderOffset.Y)}, ESlateDrawEffect::None, GridColor, true, ((FMath::RoundToInt(y/Zoom) - GridOffsetY) % 100 == 0) ? 1.0 : 0.05);
	}

	// Draw Pin Connections
	TMap<TSharedRef<SFIVSEdPinViewer>, FVector2D> ConnectionLocations; 
	TMap<UFIVSPin*, TSharedRef<SFIVSEdPinViewer>> PinMap;
	
	for (int i = 0; i < Children.Num(); ++i) {
		TSharedRef<SFIVSEdNodeViewer> Node = Children[i];
		for (int j = 0; j < Node->GetPinWidgets().Num(); ++j) {
			TSharedRef<SFIVSEdPinViewer> Pin = Node->GetPinWidgets()[j];
			ConnectionLocations.Add(Pin, Pin->GetConnectionPoint());
			PinMap.Add(Pin->GetPin(), Pin);
		}
	}

	TSet<TPair<UFIVSPin*, UFIVSPin*>> DrawnPins;
	for (int i = 0; i < Children.Num(); ++i) {
		TSharedRef<SFIVSEdNodeViewer> Node = Children[i];
		for (const TSharedRef<SFIVSEdPinViewer>& Pin : Node->GetPinWidgets()) {
			for (UFIVSPin* ConnectionPin : Pin->GetPin()->GetConnections()) {
				if (!DrawnPins.Contains(TPair<UFIVSPin*, UFIVSPin*>(Pin->GetPin(), ConnectionPin)) && !DrawnPins.Contains(TPair<UFIVSPin*, UFIVSPin*>(ConnectionPin, Pin->GetPin()))) {
					DrawnPins.Add(TPair<UFIVSPin*, UFIVSPin*>(Pin->GetPin(), ConnectionPin));
					ConnectionDrawer->DrawConnection(Pin, NodeToChild[ConnectionPin->ParentNode]->GetPinWidget(ConnectionPin), SharedThis(this), AllottedGeometry, OutDrawElements, LayerId+1);
				}
			}
		}
	}

	// draw new creating connection
	if (bIsPinDrag) {
		TSharedRef<SFIVSEdPinViewer> PinWidget = NodeToChild[PinDragStart->ParentNode]->GetPinWidget(PinDragStart);
		FVector2D StartLoc = GetCachedGeometry().AbsoluteToLocal(PinWidget->GetConnectionPoint());
		FVector2D EndLoc = PinDragEnd;
		if (PinDragStart->GetPinType() & FIVS_PIN_INPUT) {
			EndLoc = StartLoc;
			StartLoc = PinDragEnd;
		}
		ConnectionDrawer->DrawConnection(StartLoc, EndLoc, PinWidget->GetPinColor().GetSpecifiedColor(), SharedThis(this), AllottedGeometry, OutDrawElements, LayerId+100);
	}

	// Draw selection box
	if (bIsSelectionDrag) {
		FVector2D SelectStart = GetCachedGeometry().AbsoluteToLocal(SelectionDragStart);
		FVector2D SelectEnd = GetCachedGeometry().AbsoluteToLocal(SelectionDragEnd);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId+200, AllottedGeometry.ToPaintGeometry(SelectStart, SelectEnd - SelectStart), &SelectionBrush, ESlateDrawEffect::None, FLinearColor(1,1,1,0.1));
	}
	
	return ret;
}

FReply SFIVSEdGraphViewer::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
	if (!MouseEvent.GetModifierKeys().IsShiftDown() && !SelectedNodes.Contains(NodeUnderMouse)) DeselectAll();

	if (ConnectionDrawer->ConnectionUnderMouse.Key.IsValid() && MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton)) {
		TPair<TSharedPtr<SFIVSEdPinViewer>, TSharedPtr<SFIVSEdPinViewer>> Connection = ConnectionDrawer->ConnectionUnderMouse;
		TSharedPtr<IMenu> MenuHandle;
		FMenuBuilder MenuBuilder(true, NULL);
		MenuBuilder.AddMenuEntry(
            FText::FromString("Remove Connection"),
            FText(),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([Connection]() {
                Connection.Key->GetPin()->RemoveConnection(Connection.Value->GetPin());
            })));
		
		FSlateApplication::Get().PushMenu(SharedThis(this), *MouseEvent.GetEventPath(), MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);
	} else if (NodeUnderMouse) {
		if (PinUnderMouse && !MouseEvent.GetModifierKeys().IsControlDown()) {
			bIsPinDrag = true;
			PinDragStart = PinUnderMouse;
			return FReply::Handled().CaptureMouse(AsShared());
		} else {
			bIsNodeDrag = true;
			NodeDragStart = MouseEvent.GetScreenSpacePosition();
			Select(NodeUnderMouse);
			NodeDragPosStart.Empty();
			for (UFIVSNode* Node : SelectedNodes) {
				NodeDragPosStart.Add(Node->Pos);
			}
			return FReply::Handled().CaptureMouse(AsShared());
		}
	} else if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)) {
		bIsSelectionDrag = true;
		SelectionDragStart = MouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(AsShared());
	} else {
		bIsGraphDrag = true;
		GraphDragDelta = 0.0f;
		return FReply::Handled().CaptureMouse(AsShared());
	}
	return FReply::Handled();
}

FReply SFIVSEdGraphViewer::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
	if (bIsPinDrag) {
		if (PinUnderMouse) {
			bIsPinDrag = false;
			PinDragStart->AddConnection(PinUnderMouse);
		} else {
			CreateActionSelectionMenu(*MouseEvent.GetEventPath(), MouseEvent.GetScreenSpacePosition(), [this](auto){ bIsPinDrag = false; }, FFINScriptNodeCreationContext(Graph, LocalToGraph(GetCachedGeometry().AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())), PinDragStart));
		}
		return FReply::Handled().ReleaseMouseCapture();
	} else if (bIsNodeDrag) {
		bIsNodeDrag = false;
		return FReply::Handled().ReleaseMouseCapture();
	} else if (bIsSelectionDrag) {
		bIsSelectionDrag = false;
		return FReply::Handled().ReleaseMouseCapture();
	} else if (bIsGraphDrag) {
		bIsGraphDrag = false;
		if (GraphDragDelta < 10) {
			CreateActionSelectionMenu(*MouseEvent.GetEventPath(), MouseEvent.GetScreenSpacePosition(), [this](auto){}, FFINScriptNodeCreationContext(Graph, LocalToGraph(GetCachedGeometry().AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())), nullptr));
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return SPanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SFIVSEdGraphViewer::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) {
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && ConnectionDrawer.IsValid() && ConnectionDrawer->ConnectionUnderMouse.Key) {
		TPair<TSharedPtr<SFIVSEdPinViewer>, TSharedPtr<SFIVSEdPinViewer>> Connection = ConnectionDrawer->ConnectionUnderMouse;
		UFIVSPin* Pin1 = Connection.Key->GetPin();
		UFIVSPin* Pin2 = Connection.Value->GetPin();
		UFIVSRerouteNode* Node = NewObject<UFIVSRerouteNode>();
		Node->Pos = LocalToGraph(GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())) - FVector2D(10, 10);
		Pin1->RemoveConnection(Connection.Value->GetPin());
		Pin1->AddConnection(Node->GetNodePins()[0]);
		Pin2->AddConnection(Node->GetNodePins()[0]);
		Graph->AddNode(Node);
		return FReply::Handled();
	}
	return SPanel::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

FReply SFIVSEdGraphViewer::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(MyGeometry, ArrangedChildren);
	int ChildUnderMouseIndex = FindChildUnderPosition(ArrangedChildren, MouseEvent.GetScreenSpacePosition());
	if (ChildUnderMouseIndex >= 0) {
		NodeUnderMouse = Children[ChildUnderMouseIndex]->GetNode();
		PinUnderMouse = NodeToChild[NodeUnderMouse]->GetPinUnderMouse();
	} else {
		NodeUnderMouse = nullptr;
		PinUnderMouse = nullptr;
	}
	if (ConnectionDrawer) ConnectionDrawer->LastMousePosition = MouseEvent.GetScreenSpacePosition();
	if (bIsPinDrag && !ActiveActionSelection.IsValid()) {
		PinDragEnd = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	if (bIsNodeDrag) {
		for (int i = 0; i < SelectedNodes.Num(); ++i) {
			bool bSnapToGrid = !MouseEvent.GetModifierKeys().IsControlDown();
			FVector2D MoveOffset = LocalToGraph(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())) - LocalToGraph(MyGeometry.AbsoluteToLocal(NodeDragStart));
			if (bSnapToGrid) MoveOffset = FVector2D(FMath::RoundToFloat(MoveOffset.X/10.0)*10.0, FMath::RoundToFloat(MoveOffset.Y/10.0)*10.0);
			UFIVSNode* Node = SelectedNodes[i];
			Node->Pos = NodeDragPosStart[i] + MoveOffset;
			if (bSnapToGrid && SelectedNodes.Num() == 1 && !MouseEvent.GetModifierKeys().IsShiftDown()) {
				FVector2D NPos = Node->Pos / 10.0;
				Node->Pos = FVector2D(FMath::RoundToFloat(NPos.X), FMath::RoundToFloat(NPos.Y))*10.0;
			}
		}
		return FReply::Handled();
	}
	if (bIsSelectionDrag) {
		if (!FSlateApplication::Get().GetModifierKeys().IsShiftDown()) DeselectAll();
		SelectionDragEnd = MouseEvent.GetScreenSpacePosition();
		FVector2D LocalStart = GetCachedGeometry().AbsoluteToLocal(SelectionDragStart);
		FVector2D LocalEnd = GetCachedGeometry().AbsoluteToLocal(SelectionDragEnd);
		FSlateRect SelectionRect = FSlateRect(FVector2D(FMath::Min(LocalStart.X, LocalEnd.X), FMath::Min(LocalStart.Y, LocalEnd.Y)), FVector2D(FMath::Max(LocalStart.X, LocalEnd.X), FMath::Max(LocalStart.Y, LocalEnd.Y)));
		if (Graph) for (UFIVSNode* Node : Graph->GetNodes()) {
			TSharedRef<SFIVSEdNodeViewer> NodeW = NodeToChild[Node];
			FSlateRect NodeRect = FSlateRect(GetCachedGeometry().AbsoluteToLocal(NodeW->GetCachedGeometry().GetAbsolutePosition()), GetCachedGeometry().AbsoluteToLocal(NodeW->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(1,1))));
			if (Node && FSlateRect::DoRectanglesIntersect(NodeRect, SelectionRect)) {
				Select(Node);
			}
		}
		return FReply::Handled();
	}
	if (bIsGraphDrag) {
		Offset += LocalToGraph(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())) - LocalToGraph(MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition()));
		GraphDragDelta += MouseEvent.GetCursorDelta().Size();
		return FReply::Handled();
	}
	return SPanel::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SFIVSEdGraphViewer::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {
	FVector2D StartPos = LocalToGraph(GetCachedGeometry().AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	Zoom += MouseEvent.GetWheelDelta() / 10.0;
	if (Zoom < 0.1) Zoom = 0.1;
	if (Zoom > 10) Zoom = 10;
	FVector2D EndPos = LocalToGraph(GetCachedGeometry().AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	FVector2D Off = StartPos - EndPos;
	Offset -= Off;
	return FReply::Handled();
}

FReply SFIVSEdGraphViewer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) {
	if (InKeyEvent.GetKey() == EKeys::Delete) {
		if (SelectedNodes.Num() > 0) {
			for (UFIVSNode* Node : SelectedNodes) {
				Graph->RemoveNode(Node);
			}
		} else if (ConnectionDrawer && ConnectionDrawer->ConnectionUnderMouse.Key) {
			ConnectionDrawer->ConnectionUnderMouse.Key->GetPin()->RemoveConnection(ConnectionDrawer->ConnectionUnderMouse.Value->GetPin());
		}
	}
	return SPanel::OnKeyDown(MyGeometry, InKeyEvent);
}
FReply SFIVSEdGraphViewer::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) {
	return SPanel::OnKeyUp(MyGeometry, InKeyEvent);
}

bool SFIVSEdGraphViewer::IsInteractable() const {
	return true;
}

bool SFIVSEdGraphViewer::SupportsKeyboardFocus() const {
	return true;
}

FChildren* SFIVSEdGraphViewer::GetChildren() {
	return &Children;
}

void SFIVSEdGraphViewer::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const {
	for (int32 NodeIndex = 0; NodeIndex < Children.Num(); ++NodeIndex) {
		const TSharedRef<SFIVSEdNodeViewer>& Node = Children[NodeIndex];
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(Node, Node->GetPosition() + Offset, Node->GetDesiredSize(), Zoom));
	}
}

void SFIVSEdGraphViewer::OnNodeChanged(int change, UFIVSNode* Node) {
	if (change == 0) {
		CreateNodeAsChild(Node);
	} else {
		TSharedRef<SFIVSEdNodeViewer>* Viewer = NodeToChild.Find(Node);
		if (Viewer) Children.Remove(*Viewer);
	}
}

TSharedPtr<IMenu> SFIVSEdGraphViewer::CreateActionSelectionMenu(const FWidgetPath& Path, const FVector2D& Location, TFunction<void(const TSharedPtr<FFIVSEdActionSelectionAction>&)> OnExecute, const FFINScriptNodeCreationContext& Context) {
	TArray<TSharedPtr<FFIVSEdActionSelectionEntry>> Entries;
    TArray<UClass*> Derived;
    GetDerivedClasses(UObject::StaticClass(), Derived);
    for (TTuple<UClass*, UFINClass*> Clazz : FFINReflection::Get()->GetClasses()) {
    	Entries.Add(MakeShared<FFIVSEdActionSelectionTypeCategory>(Clazz.Value, Context));
    }
    TSharedRef<SFIVSEdActionSelection> Select = SNew(SFIVSEdActionSelection).OnActionExecuted_Lambda([this, OnExecute](const TSharedPtr<FFIVSEdActionSelectionAction>& Action) {
		OnExecute(Action);
    	ActiveActionSelection = nullptr;
    });
    Select->SetSource(Entries);
    TSharedPtr<IMenu> Menu = FSlateApplication::Get().PushMenu(SharedThis(this), Path, Select, Location, FPopupTransitionEffect::None);
	Select->SetMenu(Menu);
    Select->SetFocus();
	ActiveActionSelection = Select;
	Menu->GetOnMenuDismissed().AddLambda([this, OnExecute](TSharedRef<IMenu>) {
		ActiveActionSelection = nullptr;
		OnExecute(nullptr);
	});
	return Menu;
}

void SFIVSEdGraphViewer::SetGraph(UFIVSGraph* NewGraph) {
	if (Graph) {
		Graph->OnNodeChanged.Remove(OnNodeChangedHandle);
		Children.Empty();
		NodeToChild.Empty();
	}
	
	Graph = NewGraph;

	if (Graph) {
		OnNodeChangedHandle = Graph->OnNodeChanged.AddRaw(this, &SFIVSEdGraphViewer::OnNodeChanged);
		
		// Generate Nodes Children
		for (UFIVSNode* Node : Graph->GetNodes()) {
			CreateNodeAsChild(Node);
		}
	}
}

void SFIVSEdGraphViewer::CreateNodeAsChild(UFIVSNode* Node) {
	TSharedRef<SFIVSEdNodeViewer> Child = SNew(SFIVSEdNodeViewer)
        .Node(Node);
	Children.Add(Child);
	NodeToChild.Add(Node, Child);
}

void SFIVSEdGraphViewer::Select(UFIVSNode* Node) {
	if (SelectedNodes.Contains(Node)) return;
	SelectedNodes.Add(Node);
	NodeToChild[Node]->bSelected = true;
}

void SFIVSEdGraphViewer::Deselect(UFIVSNode* Node) {
	if (SelectedNodes.Remove(Node)) {
		NodeToChild[Node]->bSelected = false;
	}
}

void SFIVSEdGraphViewer::DeselectAll() {
	TArray<UFIVSNode*> Nodes = SelectedNodes;
	for (UFIVSNode* Node : Nodes) {
		Deselect(Node);
	}
}

FVector2D SFIVSEdGraphViewer::LocalToGraph(const FVector2D Local) {
	return (Local / Zoom) - Offset;
}
