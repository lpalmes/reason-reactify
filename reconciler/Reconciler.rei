open React;

let logString: string => unit;
/***
 * Log of operations performed to update an instance tree.
 */
module UpdateLog: {
  type t;
  let create: unit => t;
};

module RenderedElement: {
  /*** Type of a react element after rendering  */
  type t;
  let listToRenderedElement: list(t) => t;

  /*** Render one element by creating new instances. */
  let render: reactElement => t;

  /*** Update a rendered element when a new react element is received. */
  let update: (t, reactElement) => (t, UpdateLog.t);

  /*** Flush pending state updates (and possibly add new ones). */
  let flushPendingUpdates: t => (t, UpdateLog.t);
};

/***
 * Imperative trees obtained from rendered elements.
 * Can be updated in-place by applying an update log.
 * Can return a new tree if toplevel rendering is required.
 */
module OutputTree: {
  type t;
  let fromRenderedElement: RenderedElement.t => t;
  let applyUpdateLog: (UpdateLog.t, t) => t;
  let print: t => string;
};

module ReactDOMRe: {
  type reactDOMProps;
  let createElement:
    (string, ~props: reactDOMProps=?, array(reactElement)) => reactElement;
};