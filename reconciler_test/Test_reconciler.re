/* let div = (~children, ()) => React.(Nested("div", children));
   /***
    * The simplest component. Composes nothing!
    */
   module Box = {
     let component = React.statefulComponent("Box");
     let createElement = (~title="ImABox", ~onClick as _=?, ~children as _, ()) =>
       React.element({
         ...component,
         initialState: () => (),
         render: _self => React.stringToElement(title),
       });
   };

   module BoxWrapper = {
     let component = React.statelessComponent("BoxWrapper");
     let createElement =
         (~title="ImABox", ~twoBoxes=false, ~onClick as _=?, ~children as _, ()) =>
       React.element({
         ...component,
         initialState: () => (),
         render: _self =>
           twoBoxes ?
             <div> <Box title /> <Box title /> </div> : <div> <Box title /> </div>,
       });
   };

   /***
    * Box with dynamic keys.
    */
   module BoxWithDynamicKeys = {
     let component =
       React.statelessComponent(~useDynamicKey=true, "BoxWithDynamicKeys");
     let createElement = (~title="ImABox", ~children as _, ()) =>
       React.element({
         ...component,
         render: _self => React.stringToElement(title),
       });
   };

   module BoxList = {
     type action =
       | Create(string)
       | Reverse;
     let component = React.reducerComponent("BoxList");
     let createElement = (~rAction, ~useDynamicKeys=false, ~children as _, ()) =>
       React.element({
         ...component,
         initialState: () => [],
         reducer: (action, state) =>
           switch (action) {
           | Create(title) =>
             React.Update([
               useDynamicKeys ? <BoxWithDynamicKeys title /> : <Box title />,
               ...state,
             ])
           | Reverse => React.Update(List.rev(state))
           },
         render: ({state, send, _}) => {
           React.RemoteAction.subscribe(~send, rAction);
           React.listToElement(state);
         },
       });
   };

   /***
    * This component demonstrates several things:
    *
    * 1. Demonstration of making internal state hidden / abstract. Components
    * should encapsulate their state representation and should be free to change
    * it.
    *
    * 2. Demonstrates an equivalent of `componentWillReceiveProps`.
    * `componentWillReceiveProps` is like an "edge trigger" on props, and the
    * first item of the tuple shows how we implement that with this API.
    */
   module ChangeCounter = {
     type state = {
       numChanges: int,
       mostRecentLabel: string,
     };
     let component = React.reducerComponent("ChangeCounter");
     let createElement = (~label, ~children as _, ()) =>
       React.element({
         ...component,
         initialState: () => {mostRecentLabel: label, numChanges: 10},
         reducer: ((), state) =>
           React.Update({...state, numChanges: state.numChanges + 1000}),
         willReceiveProps: ({state, reduce, _}) =>
           label != state.mostRecentLabel ?
             {
               reduce(() => (), ());
               reduce(() => (), ());
               {mostRecentLabel: label, numChanges: state.numChanges + 1};
             } :
             state,
         render: ({state: {numChanges, mostRecentLabel, _}, _}) => {
           let title =
             Printf.sprintf("[%d, \"%s\"]", numChanges, mostRecentLabel);
           <div> <Box title /> </div>;
         },
       });
   };

   module StatelessButton = {
     let component = React.statelessComponent("StatelessButton");
     let createElement =
         (
           ~initialClickCount as _="noclicks",
           ~test as _="default",
           ~children as _,
           (),
         ) =>
       React.element({...component, render: _self => <div> <Box /> </div>});
   };

   module ButtonWrapper = {
     type state = {buttonWrapperState: int};
     let component = React.statefulComponent("ButtonWrapper");
     let createElement = (~wrappedText="default", ~children as _, ()) =>
       React.element({
         ...component,
         initialState: () => {buttonWrapperState: 0},
         render: ({state, _}) =>
           <div>
             {React.stringToElement(string_of_int(state.buttonWrapperState))}
             <StatelessButton
               initialClickCount={"wrapped:" ++ wrappedText ++ ":wrapped"}
             />
             <StatelessButton
               initialClickCount={"wrapped:" ++ wrappedText ++ ":wrapped"}
             />
           </div>,
       });
   };

   let buttonWrapperJsx = <ButtonWrapper wrappedText="TestButtonUpdated!!!" />;

   module ButtonWrapperWrapper = {
     let component = React.statefulComponent("ButtonWrapperWrapper");
     let createElement = (~wrappedText="default", ~children as _, ()) =>
       React.element({
         ...component,
         initialState: () => "buttonWrapperWrapperState",
         render: ({state, _}) =>
           <div>
             {React.stringToElement(state)}
             {React.stringToElement("wrappedText:" ++ wrappedText)}
             buttonWrapperJsx
           </div>,
       });
   };

   module UpdateAlternateClicks = {
     type action =
       | Click;
     let component = React.reducerComponent("UpdateAlternateClicks");
     let make = (~rAction, _children) => {
       ...component,
       initialState: () => 0,
       printState: state => string_of_int(state),
       reducer: (Click, state) => Update(state + 1),
       shouldUpdate: ({newSelf: {state, _}, _}) => state mod 2 === 0,
       render: ({state, send, _}) => {
         React.RemoteAction.subscribe(~send, rAction);
         React.stringToElement("Rendered state is " ++ string_of_int(state));
       },
     };
   };

   module TestUtils = {
     open React;
     open Reconciler;
     let logMsg = (~msg) => logString("\n" ++ msg ++ "\n-------------------");
     let startTest = (~msg) => {
       GlobalState.reset();
       if (msg != "") {
         logString("\n\n******* " ++ msg ++ " *******");
       };
     };

     /*** Print a renderedElement */
     let printRenderedElement = (renderedElement: RenderedElement.t) =>
       OutputTree.print(OutputTree.fromRenderedElement(renderedElement));

     /*** Print a list of renderedElement */
     let printRenderedElementList = el =>
       printRenderedElement(RenderedElement.listToRenderedElement(el));
     let renderAndPrint = (~msg, reactElement) => {
       logMsg(~msg);
       let rendered = RenderedElement.render(reactElement);
       logString(printRenderedElement(rendered));
       rendered;
     };
     let checkApplyUpdateLogs = (updateLog, renderedElement, newRenderedElement) => {
       let outputTree = OutputTree.fromRenderedElement(renderedElement);
       let newOutputTree = OutputTree.fromRenderedElement(newRenderedElement);
       let outputTreeAfterApply =
         OutputTree.applyUpdateLog(updateLog, outputTree);
       let newOutputTreeString = OutputTree.print(newOutputTree);
       let outputTreeAfterApplyString = OutputTree.print(outputTreeAfterApply);
       if (newOutputTreeString != outputTreeAfterApplyString) {
         logString("Tree expected after updateLog:");
         logString(newOutputTreeString);
         logString("Tree found:");
         logString(outputTreeAfterApplyString);
         assert(false);
       };
     };
     let updateAndPrint = (~msg, renderedElement, reactElement) => {
       logMsg(~msg);
       let (newRenderedElement, updateLog) =
         RenderedElement.update(renderedElement, reactElement);
       checkApplyUpdateLogs(updateLog, renderedElement, newRenderedElement);
       logString(printRenderedElement(newRenderedElement));
       newRenderedElement;
     };
     let flushAndPrint = (~msg, renderedElement) => {
       logMsg(~msg);
       let (newRenderedElement, updateLog) =
         RenderedElement.flushPendingUpdates(renderedElement);
       checkApplyUpdateLogs(updateLog, renderedElement, newRenderedElement);
       logString(printRenderedElement(newRenderedElement));
       newRenderedElement;
     };
     let printAll = renderedElements => {
       logMsg(~msg="Trees rendered");
       print_string(printRenderedElementList(renderedElements));
     };
   };

   module RunTest1 = {
     open TestUtils;
     startTest(~msg="Test 1");
     let rendered0 =
       renderAndPrint(~msg="BoxWrapper with one box", <BoxWrapper />);
     let rendered1 =
       updateAndPrint(
         ~msg="BoxWrapper with two boxes",
         rendered0,
         <BoxWrapper twoBoxes=true />,
       );
     printAll([rendered0, rendered1]);
   };

   module RunTest2 = {
     open TestUtils;
     startTest(~msg="Test 2");
     let rendered0 =
       renderAndPrint(
         ~msg="Initial Tree",
         <ChangeCounter label="defaultText" />,
       );
     let rendered1 =
       updateAndPrint(
         ~msg="Updating Tree With Same Label (ChangeCounter)",
         rendered0,
         <ChangeCounter label="defaultText" />,
       );
     let rendered2 =
       updateAndPrint(
         ~msg="Updating Tree With New Label (ChangeCounter)",
         rendered1,
         <ChangeCounter label="updatedText" />,
       );
     let rendered2f = flushAndPrint(~msg="Flushing (ChangeCounter)", rendered2);
     let rendered2f =
       flushAndPrint(~msg="Flushing #2 (ChangeCounter)", rendered2f);
     let rendered2f =
       flushAndPrint(~msg="Flushing #3 (ChangeCounter)", rendered2f);
     let rendered3 =
       updateAndPrint(
         ~msg="Updating Tree With New Root Type (ButtonWrapperWrapper)",
         rendered2f,
         <ButtonWrapperWrapper wrappedText="updatedText" />,
       );
     let rendered4 =
       updateAndPrint(
         ~msg=
           "Updating Tree With Same Root Type as Previous (ButtonWrapperWrapper)",
         rendered3,
         <ButtonWrapperWrapper wrappedText="updatedTextmodified" />,
       );
     printAll([
       rendered0,
       rendered1,
       rendered2,
       rendered2f,
       rendered3,
       rendered4,
     ]);
   };

   module RunTestListsWithDynamicKeys = {
     open React;
     open TestUtils;
     startTest(~msg="Test Lists With Dynamic Keys");
     let rAction = RemoteAction.create();
     let rendered0 =
       renderAndPrint(
         ~msg="Initial BoxList",
         <BoxList useDynamicKeys=true rAction />,
       );
     RemoteAction.send(rAction, ~action=BoxList.Create("Hello"));
     let rendered1 = flushAndPrint(~msg="Add Hello then Flush", rendered0);
     RemoteAction.send(rAction, ~action=BoxList.Create("World"));
     let rendered2 = flushAndPrint(~msg="Add World then Flush", rendered1);
     RemoteAction.send(rAction, ~action=BoxList.Reverse);
     let rendered3 = flushAndPrint(~msg="Reverse then Flush", rendered2);
     printAll([rendered0, rendered1, rendered2, rendered3]);
   };

   module RunTestListsWithoutDynamicKeys = {
     open React;
     open TestUtils;
     startTest(~msg="Test Lists Without Dynamic Keys");
     let rAction = RemoteAction.create();
     let rendered0 =
       renderAndPrint(
         ~msg="Initial BoxList",
         <BoxList useDynamicKeys=false rAction />,
       );
     RemoteAction.send(rAction, ~action=BoxList.Create("Hello"));
     let rendered1 = flushAndPrint(~msg="Add Hello then Flush", rendered0);
     RemoteAction.send(rAction, ~action=BoxList.Create("World"));
     let rendered2 = flushAndPrint(~msg="Add World then Flush", rendered1);
     RemoteAction.send(rAction, ~action=BoxList.Reverse);
     let rendered3 = flushAndPrint(~msg="Reverse then Flush", rendered2);
     printAll([rendered0, rendered1, rendered2, rendered3]);
   };

   module TestDeepMove = {
     open TestUtils;
     startTest(~msg="Test Deep Move Box With Dynamic Keys");
     let box = <BoxWithDynamicKeys title="box to move" />;
     let rendered0 = renderAndPrint(~msg="Initial Box", box);
     let rendered1 =
       updateAndPrint(
         ~msg="Add 2 Levels of div",
         rendered0,
         <div>
           {React.stringToElement("first level before")}
           <div>
             {React.stringToElement("second level before")}
             box
             {React.stringToElement("second level after")}
           </div>
           {React.stringToElement("first level after")}
         </div>,
       );
     printAll([rendered0, rendered1]);
   };

   /* module TestStaticKeys = {
        open TestUtils;
        startTest(~msg="Test With Static Keys");
        let key1 = React.Key.create();
        let key2 = React.Key.create();
        let rendered0 =
          renderAndPrint(
            ~msg="Initial Boxes",
            <div>
              <Box key=key1 title="Box1unchanged" />
              <Box key=key2 title="Box2unchanged" />
            </div>,
          );
        let rendered1 =
          updateAndPrint(
            ~msg="Swap Boxes",
            rendered0,
            <div>
              <Box key=key2 title="Box2changed" />
              <Box key=key1 title="Box1changed" />
            </div>,
          );
        printAll([rendered0, rendered1]);
      };

      module TestUpdateAlternateClicks = {
        open React;
        open TestUtils;
        startTest(~msg="Test Update on Alternate Clicks");
        let rAction = RemoteAction.create();
        let rendered0 =
          renderAndPrint(~msg="Initial", <UpdateAlternateClicks rAction />);
        RemoteAction.send(rAction, ~action=Click);
        let rendered1 = flushAndPrint(~msg="First click then flush", rendered0);
        RemoteAction.send(rAction, ~action=Click);
        let rendered2 = flushAndPrint(~msg="Second click then flush", rendered1);
        RemoteAction.send(rAction, ~action=Click);
        let rendered3 = flushAndPrint(~msg="Third click then flush", rendered2);
        RemoteAction.send(rAction, ~action=Click);
        let rendered4 = flushAndPrint(~msg="Fourth click then flush", rendered3);
        printAll([rendered0, rendered1, rendered2, rendered3, rendered4]);
      }; */

   /* module PerfTest = {
        open TestUtils;
        open React;
        let savedDebug = GlobalState.debug^;
        let rAction = RemoteAction.create();
        let iterationsWithoutFlush = 0;
        let iterationsWithFlush = 1000000;
        let testLogUpdate = false;
        let useDynamicKeys = true;
        let runTest = () => {
          startTest(~msg="");
          GlobalState.debug := false;
          GlobalState.useTailHack := true;
          let rendered0 =
            renderAndPrint(
              ~msg="Initial BoxList",
              <BoxList useDynamicKeys rAction />,
            );
          let currentRenderedElement = ref(rendered0);
          let currentOutputTree =
            ref(OutputTree.fromRenderedElement(currentRenderedElement^));
          for (i in 1 to iterationsWithoutFlush) {
            RemoteAction.send(rAction, ~action=BoxList.Create(string_of_int(i)));
          };
          let (newRenderedElement, updateLog) =
            RenderedElement.flushPendingUpdates(currentRenderedElement^);
          currentRenderedElement := newRenderedElement;
          currentOutputTree :=
            OutputTree.applyUpdateLog(updateLog, currentOutputTree^);
          for (i in 1 to iterationsWithFlush) {
            RemoteAction.send(rAction, ~action=BoxList.Create(string_of_int(i)));
            let (newRenderedElement, updateLog) =
              RenderedElement.flushPendingUpdates(currentRenderedElement^);
            currentRenderedElement := newRenderedElement;
            currentOutputTree :=
              OutputTree.applyUpdateLog(updateLog, currentOutputTree^);
          };
          if (testLogUpdate
              && OutputTree.print(
                   OutputTree.fromRenderedElement(currentRenderedElement^),
                 )
              != OutputTree.print(currentOutputTree^)) {
            assert(false);
          };
          GlobalState.debug := savedDebug;
        };
        /* runTest (); */
      }; */ */