# React

This is the base of all projects and it will include the foundation for all potential react-based projects in Reason.

This base package should include a ReasonReact api to promote collaboration and familiarity with people using a ReasonReact, and for the modern world of React this should also include a Hooks api that currently revery uses.

## `React` module

All blocks in Jsx are of type `React.reactElement`. This reactElement should represent:

- Strings (`React.string("Hello friend")`)
- nil/empty/null (`React.nil` | `React.null` | `React.empty`)
- List/Array (`React.list([...])` | `React.array([|...|])`)
- ReasonReact components (`let component = React.statelessComponent("Header")`)
- Functional components with Hooks (`let header = () => <div>{React.string("I'm the Header")}</div>`)

Both this expressions are equal:

```reason
<div>{React.string("Hello")</div>
Nested(div, [ Flat([String("Hello")]) ])
```

### ReasonReact components (Composite components)

This component mimics the ReasonReact api, and is the closest of the primitives to a class component in React(js).

It provides multiple lifecycles:

- didMount
- didUpdate
- willUnmount
- willUpdate
- shouldUpdate

A way to store and manipulate state:

- initialState
- reducer

And of course a `render: self => reactElement` function.

This function takes a record `self` (similar to `this` in js) that contains the state defined by the user and a send func to send actions to the reducer (a small state machine) and return a `reactElement`

#### Stateless Component

```reason
   module Greeter = {
     let component = React.statelessComponent("Greeter");

     let createElement = (~name="", ~children, ()) => {
         ...component,
         render: _self =>
         <View>
            <Text>
                {React.string("Hello " ++ name ++ "!")}
            </Text>
         </View>,
       };
   };
```

#### Reducer Component

This example was taken from the ReasonReact documentation

```reason
type state = {
  count: int,
  show: bool,
};
type action =
  | Click
  | Toggle;

let component = React.reducerComponent("Example");

let make = (~greeting, ~children, ()) => {
  initialState: () => {count: 0, show: true},
  reducer: (action, state) =>
    switch (action) {
    | Click => React.Update({...state, count: state.count + 1})
    | Toggle => React.Update({...state, show: !state.show})
    },
  render: self => {
    let message =
      "You've clicked this " ++ string_of_int(self.state.count) ++ " times(s)";
    <div>
      <button onClick=(_event => self.send(Click))>
        (React.string(message))
      </button>
      <button onClick=(_event => self.send(Toggle))>
        (React.string("Toggle greeting"))
      </button>
      (
        self.state.show
          ? React.string(greeting)
          : React.null
      )
    </div>;
  },
};
```

### Functional Components (Hooks)

So finally we are on the bleeding edge. Let's see how this components could work starting with a stateless component.

```reason
let header = (~name, ~children, ()) => {
    print_endline("I'm hooked");
    <View>
        <Text>
            {React.string("Hello " ++ name ++ "!")}
        </Text>
    </View>
};

/* This ends up like */
<Header name="Lorenzo" />
```

What about some state?

```reason
let Calculator3000 = (~children, ()) => {
    let (value, setValue) = useState(0);
    <View>
        <Button onClick={_ => setValue(value + 1)}>
          {React.string("Increment")}
        </Button>
        <Text>
          {React.string("Value: " ++ string_of_int(value))}
        </Text>
        <Button onClick={_ => setValue(value - 1)}>
          {React.string("Decrement")}
        </Button>
    </View>
};
```
