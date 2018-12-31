module HookTypes = {
  type state('a);
  type effect;
  type t('t);

  let addState: (t('t), ~state: 'state) => t(('t, state('state))) =
    (x, ~state as _) => Obj.magic(x);

  let addEffect: t('t) => t(('t, effect)) = Obj.magic;
};

let useState = (hooks, state) => {
  let setState = x => ignore(x == state);
  (HookTypes.addState(~state, hooks), state, setState);
};

let useEffect = (hooks, _functionWithEffects: unit => unit) =>
  HookTypes.addEffect(hooks);

let functionComponent = (~message, ~hooks) => {
  let (hooks, name, _setName) = useState(hooks, "Harry");
  let (hooks, surname, _setSurname) = useState(hooks, "Potter");
  let stringToRender = message ++ " " ++ name ++ " " ++ surname;
  (hooks, stringToRender);
};

let customHookNameSurname = (hooks, ~initialName, ~initialSurname) => {
  let (hooks, name, setName) = useState(hooks, initialName);
  let (hooks, surname, setSurname) = useState(hooks, initialSurname);
  (hooks, name, setName, surname, setSurname);
};

let functionComponent2 = (~message, ~hooks) => {
  let (hooks, name, _setName, surname, _setSurname) =
    customHookNameSurname(
      ~initialName="Harry",
      ~initialSurname="Potter",
      hooks,
    );
  let stringToRender = message ++ " " ++ name ++ " " ++ surname;
  (hooks, stringToRender);
};

let checkThatTheTwoFunctionsHaveTheSameType =
  ignore(functionComponent == functionComponent2);

let effectBeforeState = (~hooks) => {
  let hooks = useEffect(hooks, () => print_endline("About so use State"));
  let (hooks, _name, _setName) = useState(hooks, "Harry");
  hooks;
};

let effectAfterState = (~hooks) => {
  let (hooks, _name, _setName) = useState(hooks, "Harry");
  let hooks = useEffect(hooks, () => print_endline("Just Used State"));
  hooks;
};

/* let checkThatTheOrderMatters =
   ignore(effectBeforeState == effectAfterState); */

/* let thisDoesNotTypeCheck = (~message, ~hooks) => {
     let (hooks, _name, _setName) =
       Random.bool() ? "Harry" |> useState(hooks) : (hooks, "", _ => ());
     hooks;
   }; */

let theTypeCheckerIsSmart = (~message, ~hooks) => {
  let (hooks, name, _setName, surname, _setSurname) =
    Random.bool() ?
      {
        let (hooks, name, setName) = useState(hooks, "Harry");
        let (hooks, surname, setSurname) = useState(hooks, "Potter");
        (hooks, name, setName, surname, setSurname);
      } :
      customHookNameSurname(
        ~initialName="Harry",
        ~initialSurname="Potter",
        hooks,
      );
  let stringToRender = message ++ " " ++ name ++ " " ++ surname;
  (hooks, stringToRender);
};