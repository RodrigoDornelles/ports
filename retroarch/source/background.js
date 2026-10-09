/**
 * @brief The main loop's clock while the page is hidden in a netgame
 * (background.c).
 *
 * A hidden page gets no requestAnimationFrame and its timers slow down to
 * about once a second, but a Web Worker's timers keep their pace: while
 * the page is hidden and the game must go on, the main loop's iterations
 * wait for the worker's next tick instead.
 */
mergeInto(LibraryManager.library, {
  $DOPO_BACKGROUND__deps: ['$Browser', 'emscripten_get_now'],
  $DOPO_BACKGROUND__postset: 'DOPO_BACKGROUND.install();',
  $DOPO_BACKGROUND: {
    /* the worker's pace, a frame of a 60 Hz core */
    INTERVAL: 1000 / 60,
    keep: false,
    worker: null,
    waiting: [],
    run: null,

    /* runs func at the worker's next tick */
    later(func) {
      DOPO_BACKGROUND.waiting.push(func);
      if (DOPO_BACKGROUND.worker)
        return;
      var source = 'setInterval(function () { postMessage(0); }, ' + DOPO_BACKGROUND.INTERVAL + ');';
      var url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
      DOPO_BACKGROUND.worker = new Worker(url);
      URL.revokeObjectURL(url);
      DOPO_BACKGROUND.worker.onmessage = () => {
        var waiting = DOPO_BACKGROUND.waiting;
        DOPO_BACKGROUND.waiting = [];
        waiting.forEach((f) => f());
        if (!DOPO_BACKGROUND.waiting.length)
          DOPO_BACKGROUND.stop();
      };
    },

    stop() {
      if (!DOPO_BACKGROUND.worker)
        return;
      DOPO_BACKGROUND.worker.terminate();
      DOPO_BACKGROUND.worker = null;
    },

    /*
     * In a netgame every iteration is scheduled here, as the main loop's
     * timing mode asks (requestAnimationFrame, timeout, immediate) or, in
     * a hidden page, at the worker's next tick. Hiding or showing the page
     * hands a waiting iteration over to the other clock; its token makes
     * sure only one of them runs it.
     */
    token: null,

    schedule(mainLoop) {
      var token = DOPO_BACKGROUND.token = {};
      var run = DOPO_BACKGROUND.run = () => {
        if (DOPO_BACKGROUND.token !== token) return;
        DOPO_BACKGROUND.token = null;
        mainLoop.runner();
      };
      if (document.hidden)
        DOPO_BACKGROUND.later(run);
      else if (mainLoop.method == 'rAF')
        requestAnimationFrame(run);
      else if (mainLoop.method == 'timeout')
        setTimeout(run, Math.max(0, mainLoop.tickStartTime + mainLoop.timingValue - _emscripten_get_now()) | 0);
      else
        setTimeout(run, 0);
    },

    install() {
      var mainLoop = Browser.mainLoop;
      var scheduler = mainLoop.scheduler;
      Object.defineProperty(mainLoop, 'scheduler', {
        configurable: true,
        get() {
          if (!scheduler || !DOPO_BACKGROUND.keep)
            return scheduler;
          return () => DOPO_BACKGROUND.schedule(mainLoop);
        },
        set(value) { scheduler = value; }
      });
      document.addEventListener('visibilitychange', () => {
        if (!DOPO_BACKGROUND.token)
          return;
        if (document.hidden && DOPO_BACKGROUND.keep) {
          /* its requestAnimationFrame will not come while hidden */
          DOPO_BACKGROUND.later(DOPO_BACKGROUND.run);
        } else if (!document.hidden) {
          var waiting = DOPO_BACKGROUND.waiting;
          DOPO_BACKGROUND.waiting = [];
          DOPO_BACKGROUND.stop();
          waiting.forEach((f) => f());
        }
      });
    }
  },

  /* background.c, every iteration: whether the game goes on when hidden */
  dopo_background__deps: ['$DOPO_BACKGROUND'],
  dopo_background: function(keep_running) {
    DOPO_BACKGROUND.keep = !!keep_running;
  }
});
