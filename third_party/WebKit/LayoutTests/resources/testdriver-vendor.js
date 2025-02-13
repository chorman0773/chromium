(function() {
  "use strict";
  // Define functions one by one and do not override the whole
  // test_driver_internal as it masks the new testing fucntions
  // that will be added in the future.
  const leftButton = 0;

  function getInViewCenterPoint(rect) {
      var left = Math.max(0, rect.left);
      var right = Math.min(window.innerWidth, rect.right);
      var top = Math.max(0, rect.top);
      var bottom = Math.min(window.innerHeight, rect.bottom);

      var x = 0.5 * (left + right);
      var y = 0.5 * (top + bottom);

      return [x, y];
  }

  function getPointerInteractablePaintTree(element) {
      if (!window.document.contains(element)) {
          return [];
      }

      var rectangles = element.getClientRects();

      if (rectangles.length === 0) {
          return [];
      }

      var centerPoint = getInViewCenterPoint(rectangles[0]);

      if ("elementsFromPoint" in document) {
          return document.elementsFromPoint(centerPoint[0], centerPoint[1]);
      } else if ("msElementsFromPoint" in document) {
          var rv = document.msElementsFromPoint(centerPoint[0], centerPoint[1]);
          return Array.prototype.slice.call(rv ? rv : []);
      } else {
          throw new Error("document.elementsFromPoint unsupported");
      }
  }

  function inView(element) {
      var pointerInteractablePaintTree = getPointerInteractablePaintTree(element);
      return pointerInteractablePaintTree.indexOf(element) !== -1;
  }

  window.test_driver_internal.click = function(element, coords) {
    return new Promise(function(resolve, reject) {
      if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.pointerActionSequence(
            [{
              source: 'mouse',
              actions: [
              {name: 'pointerMove', x: coords.x, y: coords.y},
              {name: 'pointerDown', x: coords.x, y: coords.y, button: leftButton},
              {name: 'pointerUp', button: leftButton}
              ]
            }],
            resolve);
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

  window.test_driver_internal.freeze = function() {
    return new Promise(function(resolve, reject) {
      if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.freeze();
        resolve();
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

  window.test_driver_internal.action_sequence = function(actions) {
    if (window.top !== window) {
        return Promise.reject(new Error("can only send keys in top-level window"));
    }

    var didScrollIntoView = false;
    for (let i = 0; i < actions.length; i++) {
        for (let j = 0; j < actions[i].actions.length; j++) {
            if ('origin' in actions[i].actions[j]) {
                if (actions[i].actions[j].origin == "viewport")
                  continue;

                if (actions[i].actions[j].origin == "pointer")
                  return Promise.reject(new Error("pointer origin is not supported right now"));

                let element = actions[i].actions[j].origin;
                if (!window.document.contains(element)) {
                    return Promise.reject(new Error("element in different document or shadow tree"));
                }

                if (!inView(element)) {
                    if (didScrollIntoView)
                      return Promise.reject(new Error("already scrolled into view, the element is not found"));

                    element.scrollIntoView({behavior: "instant",
                                            block: "end",
                                            inline: "nearest"});
                    didScrollIntoView = true;
                }

                var pointerInteractablePaintTree = getPointerInteractablePaintTree(element);
                if (pointerInteractablePaintTree.length === 0 ||
                    !element.contains(pointerInteractablePaintTree[0])) {
                    return Promise.reject(new Error("element click intercepted error"));
                }

                var rect = element.getClientRects()[0];
                var centerPoint = getInViewCenterPoint(rect);
                var x = centerPoint[0];
                var y = centerPoint[1];
                actions[i].actions[j].x += x;
                actions[i].actions[j].y += y;
            }
        }
    }

    return new Promise(function(resolve, reject) {
      if (window.chrome && chrome.gpuBenchmarking) {
        chrome.gpuBenchmarking.pointerActionSequence(actions, resolve);
      } else {
        reject(new Error("GPU benchmarking is not enabled."));
      }
    });
  };

})();
