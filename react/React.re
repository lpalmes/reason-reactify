module GlobalState = {
  let debug = ref(true);
  let componentKeyCounter = ref(0);
  let instanceIdCounter = ref(0);
  let reset = () => {
    debug := true;
    componentKeyCounter := 0;
    instanceIdCounter := 1; /* id 0 reserved for defaultStringInstance */
  };
  /*
   * Use physical equality to recognize that an element was added to the list of children.
   * Note: this currently does not check for pending updates on components in the list.
   */
  let useTailHack = ref(false);
};

module Key = {
  type t = int;
  let none = (-1);
  let first = 0;
  let create = () => {
    incr(GlobalState.componentKeyCounter);
    GlobalState.componentKeyCounter^;
  };
};

type sideEffects = unit => unit;

type stateless = unit;

type actionless = unit;

/* Phantom type to satisfy handOffInstance */
type instancePhantom;

module Callback = {
  type t('payload) = 'payload => unit;
  let default = _event => ();
  let chain = (handlerOne, handlerTwo, payload) => {
    handlerOne(payload);
    handlerTwo(payload);
  };
};

type reduce('payload, 'action) =
  ('payload => 'action) => Callback.t('payload);

type update('state, 'action) =
  | NoUpdate
  | Update('state)
and self('state, 'action) = {
  state: 'state,
  reduce: 'payload. reduce('payload, 'action),
  send: 'action => unit,
}
/***
 * Elements are what JSX blocks become. They represent the *potential* for a
 * component instance and state to be created / updated. They are not yet
 * instances.
 */
and element =
  | Element(component('state, 'action)): element
  | String(string): element
/***
 * We will want to replace this with a more efficient data structure.
 */
and reactElement =
  | Flat(list(element))
  | Nested(string, list(reactElement))
and oldNewSelf('state, 'action) = {
  oldSelf: self('state, 'action),
  newSelf: self('state, 'action),
}
and componentSpec('state, 'initialState, 'action) = {
  debugName: string,
  willReceiveProps: self('state, 'action) => 'state,
  didMount: self('state, 'action) => unit,
  didUpdate: oldNewSelf('state, 'action) => unit,
  willUnmount: self('state, 'action) => unit /* TODO: currently unused */,
  willUpdate: oldNewSelf('state, 'action) => unit,
  shouldUpdate: oldNewSelf('state, 'action) => bool,
  render: self('state, 'action) => reactElement,
  initialState: unit => 'initialState,
  reducer: ('action, 'state) => update('state, 'action),
  printState: 'state => string /* for internal debugging */,
  handedOffInstance: ref(option(instancePhantom)), /* Used to avoid Obj.magic in update, */
  key: int,
}
and component('state, 'action) = componentSpec('state, 'state, 'action);

let defaultShouldUpdate = _oldNewSef => true;

let defaultWillUpdate = _oldNewSef => ();

let defaultDidUpdate = _oldNewSef => ();

let defaultDidMount = _self => ();

let basicComponent =
    (~useDynamicKey=false, debugName): componentSpec(_, stateless, _) => {
  let key = useDynamicKey ? Key.first : Key.none;
  {
    debugName,
    willReceiveProps: ({state, _}) => state,
    didMount: defaultDidMount,
    didUpdate: defaultDidUpdate,
    willUnmount: _self => (),
    willUpdate: defaultWillUpdate,
    shouldUpdate: defaultShouldUpdate,
    render: _self => assert(false),
    initialState: () => (),
    reducer: (_action, _state) => NoUpdate,
    printState: _state => "",
    handedOffInstance: ref(None),
    key,
  };
};

let statelessComponent = (~useDynamicKey=?, debugName) => {
  ...basicComponent(~useDynamicKey?, debugName),
  initialState: () => (),
};

let statefulComponent = (~useDynamicKey=?, debugName) =>
  basicComponent(~useDynamicKey?, debugName);

let reducerComponent = (~useDynamicKey=?, debugName) =>
  basicComponent(~useDynamicKey?, debugName);

let element = (~key as argumentKey=Key.none, component) => {
  let key =
    argumentKey != Key.none ?
      argumentKey : component.key == Key.none ? Key.none : Key.create();
  let componentWithKey = key == Key.none ? component : {...component, key};
  Flat([Element(componentWithKey)]);
};

let arrayToElement = (a: array(reactElement)): reactElement =>
  Nested("Array", Array.to_list(a));

let listToElement = l => Nested("List", l);

let stringToElement = (s): reactElement => Flat([String(s)]);

module RemoteAction = {
  type t('action) = {mutable send: 'action => unit};
  let sendDefault = _action => ();
  let create = () => {send: sendDefault};
  let subscribe = (~send, x) =>
    if (x.send === sendDefault) {
      x.send = send;
    };
  let send = (x, ~action) => x.send(action);
};