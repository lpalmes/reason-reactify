open Reactify_Types;

module Make = (ReconcilerImpl: Reconciler) => {
  /* Module to give us unique IDs for components */
  type renderedElement =
    | RenderedPrimitive(ReconcilerImpl.node)
  and elementWithChildren = (childComponents, Effects.effects, Context.t)
  and render = unit => elementWithChildren
  and component =
    | Primitive(ReconcilerImpl.primitives, render)
    | Component(ComponentId.t, render)
    | Provider(render)
    | Empty(render)
  and componentFunction = unit => component
  and childComponents = list(component)
  /*
      An instance is a component that has been rendered.
      We store some additional context for it, like the state,
      effects that need to be run, and corresponding nodes.
   */
  and instance = {
    mutable component,
    children: childComponents,
    node: option(ReconcilerImpl.node),
    rootNode: ReconcilerImpl.node,
    mutable childInstances,
    mutable effectInstances: Effects.effectInstances,
    state: State.HeterogenousMutableList.t,
    context: Context.HeterogenousHashtbl.t,
    container: t,
  }
  and container = {
    onBeginReconcile: Event.t(ReconcilerImpl.node),
    onEndReconcile: Event.t(ReconcilerImpl.node),
    rootInstance: ref(option(instance)),
    containerNode: ReconcilerImpl.node,
  }
  and t = container
  and childInstances = list(instance);

  type node = ReconcilerImpl.node;
  type primitives = ReconcilerImpl.primitives;

  /*
     A global, non-pure container to hold effects
     during the course of a render operation.
   */
  let __globalEffects = Effects.create();

  let _uniqueIdScope = ComponentId.createScope();

  /*
      A global, non-pure container to hold current
      context during hte course of a render operation.
   */
  let noContext = Context.create();
  let __globalContext = ref(noContext);

  /*
     State management for reconciliation
   */
  module ComponentStateContext = {
    type t = instance;
  };
  module ComponentState = State.Make(ComponentStateContext);
  let noState = ComponentState.create([]);
  let __globalState = ref(noState);

  /*
     Container API
   */
  type reconcileNotification = node => unit;

  let createContainer =
      (
        ~onBeginReconcile: option(reconcileNotification)=?,
        ~onEndReconcile: option(reconcileNotification)=?,
        rootNode: ReconcilerImpl.node,
      ) => {
    let be = Event.create();
    let ee = Event.create();

    switch (onBeginReconcile) {
    | Some(x) =>
      let _ = Event.subscribe(be, x);
      ();
    | _ => ()
    };

    switch (onEndReconcile) {
    | Some(x) =>
      let _ = Event.subscribe(ee, x);
      ();
    | _ => ()
    };

    let ret: container = {
      onBeginReconcile: be,
      onEndReconcile: ee,
      containerNode: rootNode,
      rootInstance: ref(None),
    };

    ret;
  };

  let empty: component = Empty(() => ([], [], __globalContext^));

  type renderFunction =
    (~children: list(component)=?, componentFunction) => component;
  let render = (id: ComponentId.t, ~children: option(list(component))=?, c) => {
    ignore(children);
    let ret: component =
      Component(
        id,
        () => {
          Effects.resetEffects(__globalEffects);
          let children: list(component) = [c()];
          let effects = Effects.getEffects(__globalEffects);
          let renderResult: elementWithChildren = (
            children,
            effects,
            __globalContext^,
          );
          renderResult;
        },
      );
    ret;
  };

  module type Component = {
    type t;
    /* let derp: unit => unit; */
    let createElement: t;
  };

  type func('a) = renderFunction => 'a;

  let component = (type a, fn): (module Component with type t = a) => {
    let id = ComponentId.newId(_uniqueIdScope);
    let boundFunc = fn(render(id));
    (module
     {
       type t = a;
       let createElement = boundFunc;
     });
  };

  let primitiveComponent = (~children, prim) => {
    let comp: component =
      Primitive(prim, () => (children, [], __globalContext^));
    comp;
  };

  /* Context */
  let __contextId = ref(0);
  type providerConstructor('t) =
    (~children: childComponents, ~value: 't, unit) => component;
  type context('t) = {
    initialValue: 't,
    id: int,
  };

  let createContext = (initialValue: 't) => {
    let contextId = __contextId^;
    __contextId := __contextId^ + 1;
    let ret: context('t) = {initialValue, id: contextId};
    ret;
  };

  let getProvider = ctx => {
    let provider = (~children, ~value, ()) => {
      let ret: component =
        Provider(
          () => {
            let contextId = ctx.id;
            let context = Context.clone(__globalContext^);
            Context.set(context, contextId, Object.to_object(value));
            (children, [], context);
          },
        );
      ret;
    };
    provider;
  };

  let useContext = (ctx: context('t)) =>
    switch (Context.get(__globalContext^, ctx.id)) {
    | Some(x) => Object.of_object(x)
    | None => ctx.initialValue
    };

  let useEffect =
      (
        ~condition: Effects.effectCondition=Effects.Always,
        e: Effects.effectFunction,
      ) => {
    Effects.addEffect(~condition, __globalEffects, e);
    ();
  };

  let _getEffectsFromInstance = (instance: option(instance)) =>
    switch (instance) {
    | None => []
    | Some(i) => i.effectInstances
    };

  let _getPreviousChildInstances = (instance: option(instance)) =>
    switch (instance) {
    | None => []
    | Some(i) => i.childInstances
    };

  let _getCurrentStateFromInstance = (instance: option(instance)) =>
    switch (instance) {
    | None => []
    | Some(i) => i.state
    };

  let rec getFirstNode = (node: instance) =>
    switch (node.node) {
    | Some(n) => Some(n)
    | None =>
      switch (node.childInstances) {
      | [] => None
      | [c] => getFirstNode(c)
      | _ => None
      }
    };

  let isInstanceOfComponent =
      (instance: option(instance), component: component) =>
    switch (instance) {
    | None => false
    | Some(x) =>
      switch (x.component, component) {
      | (Primitive(a, _), Primitive(b, _)) =>
        Utility.areConstructorsEqual(a, b)
      | (Component(a, _), Component(b, _)) => a === b
      | _ => x.component === component
      }
    };

  /*
   * Instantiate turns a component function into a live instance,
   * and asks the reconciler to append it to the root node.
   */
  let rec instantiate =
          (
            rootNode,
            previousInstance: option(instance),
            component: component,
            context: Context.t,
            container: t,
          ) => {
    let previousState = ref([]);

    /* Recycle any previous effect instances */
    let previousEffectInstances = _getEffectsFromInstance(previousInstance);

    let isSameInstanceAsBefore =
      isInstanceOfComponent(previousInstance, component);

    if (isSameInstanceAsBefore) {
      /* Set up state for the component */
      previousState := _getCurrentStateFromInstance(previousInstance);
    };

    let state = ComponentState.create(previousState^);
    /* We hold onto a reference to the component instance - we need to set this _after_ the component is instantiated */
    let stateContext = ComponentState.getCurrentContext(state);

    /*
         This is dirty, but we set the 'global' state so that the 'useState'
         can have access to it, without additional binding or passing. This is
         necessary to preserve the semantics of the React-style API
     */
    __globalState := state;
    __globalContext := context;
    let (children, effects, newContext) =
      switch (component) {
      | Primitive(_, render)
      | Component(_, render)
      | Provider(render)
      | Empty(render) => render()
      };
    /* Once rendering is complete, we don't need this anymore */
    __globalContext := noContext;
    __globalState := noState;
    let newState = ComponentState.getNewState(state);

    let newEffectCount = List.length(effects);

    let newEffectInstances =
      isSameInstanceAsBefore ?
        Effects.runEffects(
          ~previousInstances=previousEffectInstances,
          effects,
        ) :
        {
          Effects.drainEffects(previousEffectInstances);
          let emptyInstances =
            Effects.createEmptyEffectInstances(newEffectCount);
          Effects.runEffects(~previousInstances=emptyInstances, effects);
        };

    let primitiveInstance =
      switch (component) {
      | Primitive(p, _render) => Some(ReconcilerImpl.createInstance(p))
      | _ => None
      };

    let nextRootPrimitiveInstance =
      switch (primitiveInstance) {
      | Some(i) => i
      | None => rootNode
      };

    let previousChildInstances = _getPreviousChildInstances(previousInstance);
    let childInstances =
      reconcileChildren(
        nextRootPrimitiveInstance,
        previousChildInstances,
        children,
        newContext,
        container,
      );

    let instance: instance = {
      component,
      node: primitiveInstance,
      rootNode: nextRootPrimitiveInstance,
      children,
      childInstances,
      effectInstances: newEffectInstances,
      state: newState,
      context: newContext,
      container,
    };

    /*
         'context' is the instance that state needs when 'setState' is called
         We set it here, after the instance is fully realized, so that the 'setState'
         callback has the latest state for the component instance.
     */
    stateContext := Some(instance);

    instance;
  }
  and reconcile = (rootNode, instance, component, context, container) => {
    let newInstance =
      instantiate(rootNode, instance, component, context, container);

    let r =
      switch (instance) {
      | None =>
        switch (newInstance.node) {
        | Some(n) => ReconcilerImpl.appendChild(rootNode, n)
        | None => ()
        };

        newInstance;
      | Some(i) =>
        let ret =
          switch (newInstance.node, i.node) {
          | (Some(a), Some(b)) =>
            /* Only both replacing node if the primitives are different */
            switch (newInstance.component, i.component) {
            | (Primitive(newPrim, _), Primitive(oldPrim, _)) =>
              if (oldPrim !== newPrim) {
                /* Check if the primitive type is the same - if it is, we can simply update the node */
                /* If not, we'll replace the node */
                if (Utility.areConstructorsEqual(oldPrim, newPrim)) {
                  ReconcilerImpl.updateInstance(b, oldPrim, newPrim);
                  i.component = newInstance.component;
                  i.effectInstances = newInstance.effectInstances;
                  i.childInstances =
                    reconcileChildren(
                      b,
                      i.childInstances,
                      newInstance.children,
                      context,
                      container,
                    );
                  i;
                } else {
                  ReconcilerImpl.replaceChild(rootNode, a, b);
                  newInstance;
                };
              } else {
                /* The node itself is unchanged, so we'll just reconcile the children */
                i.effectInstances = newInstance.effectInstances;
                i.childInstances =
                  reconcileChildren(
                    b,
                    i.childInstances,
                    newInstance.children,
                    context,
                    container,
                  );
                i;
              }
            | _ =>
              print_endline(
                "ERROR: Should only be nodes if there are primitives!",
              );
              newInstance;
            }
          | (Some(a), None) =>
            /* If there was a non-primitive instance, we need to get the top-level node - */
            /* and then remove it */
            let currentNode = getFirstNode(i);
            switch (currentNode) {
            | Some(c) => ReconcilerImpl.removeChild(rootNode, c)
            | _ => ()
            };
            ReconcilerImpl.appendChild(rootNode, a);
            newInstance;
          | (None, Some(b)) =>
            ReconcilerImpl.removeChild(rootNode, b);
            newInstance;
          | (None, None) => newInstance
          };

        ret;
      };
    r;
  }
  and reconcileChildren =
      (
        root: node,
        currentChildInstances: childInstances,
        newChildren: list(component),
        context: Context.t,
        container: t,
      ) => {
    let currentChildInstances: array(instance) =
      Array.of_list(currentChildInstances);
    let newChildren = Array.of_list(newChildren);

    let newChildInstances: ref(childInstances) = ref([]);

    for (i in 0 to Array.length(newChildren) - 1) {
      let childInstance =
        i >= Array.length(currentChildInstances) ?
          None : Some(currentChildInstances[i]);
      let childComponent = newChildren[i];
      let newChildInstance =
        reconcile(root, childInstance, childComponent, context, container);
      newChildInstances :=
        List.append(newChildInstances^, [newChildInstance]);
    };

    /* Clean up existing children */
    for (i in
         Array.length(newChildren) to
         Array.length(currentChildInstances) - 1) {
      switch (currentChildInstances[i].node) {
      | Some(n) => ReconcilerImpl.removeChild(root, n)
      | _ => ()
      };
    };

    newChildInstances^;
  };

  let useReducer =
      (reducer: ('state, 'action) => 'state, initialState: 'state) => {
    print_endline("Using reducer");
    let globalState = __globalState^;
    let componentState =
      ComponentState.popOldState(globalState, initialState);

    let (getState, updateState) =
      ComponentState.pushNewState(globalState, componentState);

    let currentContext = ComponentState.getCurrentContext(globalState);

    let dispatch = (context: ref(option(instance)), action: 'action) => {
      let newVal = reducer(getState(), action);
      updateState(newVal);
      switch (context^) {
      | Some(i) =>
        let {rootNode, component, _} = i;
        Event.dispatch(i.container.onBeginReconcile, rootNode);
        let _ =
          reconcile(rootNode, Some(i), component, i.context, i.container);
        Event.dispatch(i.container.onEndReconcile, rootNode);
        ();
      | _ => print_endline("WARNING: Skipping reconcile!")
      };
    };

    (componentState, dispatch(currentContext));
  };

  type useStateAction('a) =
    | SetState('a);
  let useStateReducer = (_state, action) =>
    switch (action) {
    | SetState(newState) => newState
    };
  let useState = initialState => {
    let (componentState, dispatch) =
      useReducer(useStateReducer, initialState);
    let setState = newState => dispatch(SetState(newState));
    (componentState, setState);
  };

  let updateContainer = (container, component) => {
    let {containerNode, rootInstance, onBeginReconcile, onEndReconcile} = container;
    let prevInstance = rootInstance^;
    Event.dispatch(onBeginReconcile, containerNode);
    let nextInstance =
      reconcile(containerNode, prevInstance, component, noContext, container);
    rootInstance := Some(nextInstance);
    Event.dispatch(onEndReconcile, containerNode);
  };
};

module State = State;
module Event = Event;
module Utility = Utility;
module Object = Object;