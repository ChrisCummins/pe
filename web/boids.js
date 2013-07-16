var Boids = Boids || {};

(function() {
  'use strict';

  /* The configuration */
  var config = {

    SIMULATION: {
      /* The minimum target frame rate */
      minFps: 30,

      /* A higher physics frame rate will increase the granularity of the
       * simulation at the expense of greater computational costs. */
      physicsFps: 50
    },

    BOUNDARY: {
      /* The size of the bounding area: */
      size: new THREE.Vector3(600, 400, 600),
      /* The threshold at which boids steer to
       * avoid the boundaries: */
      threshold: 0.30,
      /* The rate at which boids are repelled
       * from the boundaries: */
      rate: 0.00003
    },

    BOIDS: {
      /* The number of boids: */
      count: 40,
      /* Rules determining the boids behaviour: */
      behaviour: {
        /* The rate at which boids are
         * attracted: */
        cohesion: 0.00005,
        /* The amount that boids align
         * their flights: */
        alignment: 0.010,
        separation: {
          /* The distance at which boids
           * steer to avoid each other: */
          distance: 100,
          /* The rate at which boids are
           * repelled from one another: */
          rate: 0.0020
        }
      },
      /* The speed limits for boids: */
      speed: {
        min: 0.15,
        max: 0.35
      },
      /* The distance that boids can see: */
      los: 160,
    }
  };

  var _dt = 1000 / config.SIMULATION.physicsFps;

  /* The context */
  var context = {

    container: document.getElementById('container'),

    boids: [],

    SCENE: {
      /* The scene and associated entities and geometries: */
      s: null,
      camera: null,
      cameraTarget: null,
      cameraHeight: 0,
      boundary: null,
      lights: [],
      firstPerson: false
    },

    RENDERER: {
      /* The renderer and associated properties: */
      r: new THREE.CanvasRenderer(),
      width: 1.0,
      height: 0.65,
    },

    TIME: {
      /* The delta time, aka. the size of the chunk of time to process for each
       * update of the simulation. */
      dt: _dt,
      /* The maximum amount of time to process for a given iteration of the render
       * loop. The time is capped at this value, meaning that the simulation will
       * begin to slow down if the tick time exceeds this value.  */
      maxTickTime: 1000 / config.SIMULATION.minFps,
      /* The current time */
      current: new Date().getTime(),
      /* The accumulated error between the frequency of ticks and render
       * updates */
      accumulator: 0,
    },

    CONSTANTS: {
      cohesionForce: config.BOIDS.behaviour.cohesion * _dt,
      boundaryForce: config.BOUNDARY.rate * _dt,
      minSpeed: config.BOIDS.speed.min * _dt,
      maxSpeed: config.BOIDS.speed.max * _dt
    }
  };

  /* Boid behaviour */
  function Boid() {

    /* Bird.js - from the three.js birds demo */
    var Bird = function() {

      function v(x, y, z) {
        scope.vertices.push(new THREE.Vector3(x, y, z));
      }

      function f3(a, b, c) {
        scope.faces.push(new THREE.Face3(a, b, c));
      }

      var scope = this;

      THREE.Geometry.call(this);

      v(10, 0, 0);
      v(-10, -4, 2);
      v(-10, 0, 0);
      v(-10, -4, -2);

      v(0, 4, -12);
      v(0, 4, 12);
      v(4, 0, 0);
      v(-6, 0, 0);

      f3(0, 2, 1);
      f3(4, 7, 6);
      f3(5, 6, 7);

      this.computeCentroids();
      this.computeFaceNormals();
    };

    Bird.prototype = Object.create(THREE.Geometry.prototype);

    var geometry = new Bird();

    this.mesh = new THREE.Mesh(geometry,
                               new THREE.MeshLambertMaterial({
                                 color: 0xffffff,
                                 shading: THREE.FlatShading,
                                 side: THREE.DoubleSide,
                                 overdraw: true
                               }));
    this.phase = Math.floor(Math.random() * 62.83);
    this.speed = 1;
    this.shadow = new THREE.Mesh(geometry,
                                 new THREE.MeshLambertMaterial({
                                   color: 0x000000,
                                   shading: THREE.FlatShading,
                                   side: THREE.DoubleSide,
                                   opacity: 0.1,
                                   overdraw: true
                                 }));
    this.shadow.y = -config.BOUNDARY.size.y;

    this.position = new THREE.Vector3(Math.random() * boundaries.x,
                                      Math.random() * boundaries.y,
                                      Math.random() * boundaries.z)
      .multiplyScalar(2).sub(boundaries);

    this.velocity = new THREE.Vector3(Math.random() - 0.5,
                                      Math.random() - 0.5,
                                      Math.random() - 0.5)
      .multiplyScalar(4); /* This multiplication by a magic constant
                           * determines the maximum starting speed. */

    this.updateMesh();
    context.SCENE.s.add(this.mesh);
    context.SCENE.s.add(this.shadow);
  };

  Boid.prototype.updateMesh = function() {
    /* Position */
    this.mesh.position.copy(this.position);
    this.mesh.rotation.y = Math.atan2(-this.velocity.z, this.velocity.x);
    this.mesh.rotation.z = Math.asin(this.velocity.y / this.speed);

    /* FIXME: what a total hack (!) */
    if (isNaN(this.mesh.rotation.z))
      this.mesh.rotation.z = 0;

    /* Wing flapping */
    this.phase += Math.max(0, this.mesh.rotation.z * 0.8) + 0.06;
    this.phase %= 62.83;

    var wingY = Math.sin(this.phase) * 10;

    this.mesh.geometry.vertices[4].y = wingY;
    this.mesh.geometry.vertices[5].y = wingY;

    /* The Shadow */
    if (this.mesh.position.y > -config.BOUNDARY.size.y) {
      this.shadow.material.opacity = 0.1;

      this.shadow.position.copy(this.mesh.position);
      this.shadow.position.y = -config.BOUNDARY.size.y;

      this.shadow.rotation.copy(this.mesh.rotation);

      this.shadow.geometry.vertices[4].y = this.mesh.geometry.vertices[4].y;
      this.shadow.geometry.vertices[5].y = this.mesh.geometry.vertices[5].y;
    } else {
      this.shadow.material.opacity = 0;
    }
  };

  var boundaries;

  function createBoid() {
    context.boids.push(new Boid());
  }

  function destroyBoid() {
    var b = context.boids.pop();

    context.SCENE.s.remove(b.mesh);
    context.SCENE.s.remove(b.shadow);
  }

  /* Update function */
  function tick(timestamp) {

    function update(dt) {

      function updateBoid(index) {

        function enforceSpeedLimits(v) {
          var speed = v.length();

          b.speed = speed;

          if (speed > context.CONSTANTS.maxSpeed) {

            /* Maximum speed */
            b.speed = context.CONSTANTS.maxSpeed;

            v.multiplyScalar(context.CONSTANTS.maxSpeed / speed);
          } else if (speed < context.CONSTANTS.minSpeed) {

            /* Minimum speed */
            b.speed = context.CONSTANTS.minSpeed;
            speed = Math.max(speed, 0.0001);

            v.multiplyScalar(context.CONSTANTS.minSpeed / speed);
          }
        }

        var b = context.boids[index];
        var dv = new THREE.Vector3(0, 0, 0); // Change in velocity
        var centerOfMass = new THREE.Vector3(0, 0, 0);
        var velocityAvg = new THREE.Vector3(0, 0, 0);
        var swarmSize = 0;

        for (var i = 0; i < config.BOIDS.count; i++) {
          if (i !== index) {
            var otherBoid = context.boids[i];

            var dp = new THREE.Vector3().subVectors(b.position,
                                                    otherBoid.position);

            /* Get the distance between the other Boid and this Boid */
            var distance = dp.length();

            /*
             * COLLISION AVOIDANCE
             *
             * Boids try to keep a small distance away from other boids to
             * prevent them bumping into each other and reduce the density of
             * the flock:
             */
            if (distance < config.BOIDS.behaviour.separation.distance) {
              dv.sub(new THREE.Vector3()
                     .subVectors(otherBoid.position, b.position)
                     .multiplyScalar(config.BOIDS.behaviour.separation.rate));
            }

            /* We total up the velocity and positions of any particles that are
             * within the range of visibility of the current particle:
             */
            if (distance < config.BOIDS.los) {
              centerOfMass.add(otherBoid.position);
              velocityAvg.add(otherBoid.velocity);

              swarmSize++;
            }
          }
        }

        /* We must always have a flock to compare against, even
         * if a Boid is on it's own: */
        if (swarmSize < 1) {
          centerOfMass.copy(b.position);
          swarmSize = 1;
        }

        centerOfMass.divideScalar(swarmSize);
        velocityAvg.divideScalar(swarmSize);

        /*
         * PARTICLE COHESION
         *
         * Boids try to fly towards the centre of mass of neighbouring
         * boids. We do this by first calculating a 'center of mass' for the
         * flock, and moving the boid by an amount proportional to it's
         * distance from that center:
         */
        dv.add(new THREE.Vector3()
               .subVectors(centerOfMass, b.position)
               .multiplyScalar(context.CONSTANTS.cohesionForce));

        /*
         * FLOCK ALIGNMENT
         *
         * Boids try to match velocity with other boids nearby, this creates a
         * pattern of cohesive behaviour, with the flock moving in unison:
         */
        dv.add(new THREE.Vector3()
               .subVectors(velocityAvg, b.velocity)
               .multiplyScalar(config.BOIDS.behaviour.alignment));

        /*
         * BOUNDARY AVOIDANCE
         *
         * Boids avoid boundaries by being negatively accelerated away from
         * them when the distance to the boundary is less than a known
         * threshold:
         */
        if (b.position.x < -boundaries.x)
          dv.x += (-boundaries.x - b.position.x) *
            context.CONSTANTS.boundaryForce * b.speed;
        else if (b.position.x > boundaries.x)
          dv.x += (boundaries.x - b.position.x) *
            context.CONSTANTS.boundaryForce * b.speed;

        if (b.position.y < -boundaries.y)
          dv.y += (-boundaries.y - b.position.y) *
            context.CONSTANTS.boundaryForce * b.speed;
        else if (b.position.y > boundaries.y)
          dv.y += (boundaries.y - b.position.y) *
            context.CONSTANTS.boundaryForce * b.speed;

        if (b.position.z < -boundaries.z)
          dv.z += (-boundaries.z - b.position.z) *
            context.CONSTANTS.boundaryForce * b.speed;
        else if (b.position.z > boundaries.z)
          dv.z += (boundaries.z - b.position.z) *
            context.CONSTANTS.boundaryForce * b.speed;

        /* Apply the velocity change */
        b.velocity.add(dv);

        /* Control the rate of boids movement */
        enforceSpeedLimits(b.velocity);

        /* Update position */
        b.position.add(b.velocity);

        b.updateMesh();
      }

      for (var i = 0; i < config.BOIDS.count; i++)
        updateBoid(i);
    }

    function render() {
      var camera = context.SCENE.camera;

      if (context.SCENE.firstPerson) {
        /* First person camera mode. We use the first boid as our "cameraman",
         * since he is always guaranteed to be around (if there was only 1 boid,
         * he'd be the only one left). We point the camera in the direction of
         * flight by using the boid's velocity to calculate its next position
         * and pointing that camera at it.
         */
        var b = context.boids[0];
        var nextPos = new THREE.Vector3().copy(b.position).add(b.velocity);

        camera.position = new THREE.Vector3().copy(b.position);
        camera.lookAt(nextPos);
      } else{
        /* Normal (3rd person) camera mode. Gently pan the camera around */
        var timer = Date.now() * 0.00005;
        var x = Math.cos(timer) * config.BOUNDARY.size.x * 1.75;
        var z = Math.sin(timer) * config.BOUNDARY.size.z * 1.75;

        camera.position = new THREE.Vector3(x, context.SCENE.cameraHeight, z);
        camera.lookAt(context.SCENE.cameraTarget);
      }

      context.RENDERER.r.render(context.SCENE.s, camera);
    }

    /* Update the clocks */
    var t = context.TIME;
    var newTime = new Date().getTime();
    var tickTime = newTime - t.current;

    /* Enforce a maximum frame time to prevent the "spiral of death" when
     * operating under heavy load */
    if (tickTime > t.maxTickTime)
      tickTime = t.maxTickTime;

    t.current = newTime;
    t.accumulator += tickTime;

    /* Update the simulation state as required */
    while (t.accumulator >= t.dt) {
      update(t.dt);
      t.accumulator -= t.dt;
    }

    /* Render the new state */
    render();

    /* Request a new tick */
    requestAnimationFrame(tick);
  }

  function initLighting() {

    var lights = context.SCENE.lights;
    var scene = context.SCENE.s;

    if (lights.length > 0) {
      /* Clear lights */
      for (var i = lights.length - 1; i >= 0; i--)
        scene.remove(lights.pop());
    }

    var ambientLight = new THREE.AmbientLight(0x111133);
    scene.add(ambientLight);
    lights.push(ambientLight);

    var light = new THREE.DirectionalLight(0x7a2338);
    light.position.set(Math.random() - 0.5,
                       Math.random() - 0.5,
                       Math.random() - 0.5);
    light.position.normalize();
    scene.add(light);

    lights.push(light);

    var light = new THREE.DirectionalLight(0x21d592);
    light.position.set(Math.random() - 0.5,
                       Math.random() - 0.5,
                       Math.random() - 0.5);
    light.position.normalize();
    scene.add(light);

    lights.push(light);
  }

  function setRendererSize() {
    var w = container.offsetWidth * context.RENDERER.width;
    var h = window.innerHeight * context.RENDERER.height;
    var aspect = window.innerWidth / window.innerHeight;

    context.RENDERER.r.setSize(w, h);
    context.SCENE.camera.aspect = aspect;
  }

  /* Initialisation function */
  function init() {

    function initCamera() {
      /* Camera configuration */
      var fov = 60;
      var aspect = window.innerWidth / window.innerHeight;
      var zNear = 1;
      var zFar = 10000;

      context.SCENE.camera = new THREE.PerspectiveCamera(fov, aspect,
                                                         zNear, zFar);
      context.SCENE.cameraTarget = new THREE.Vector3(context.SCENE.s.position.x,
                                                     context.SCENE.s.position.y,
                                                     context.SCENE.s.position.z);
      context.SCENE.cameraHeight = config.BOUNDARY.size.y * 0.4;
    }

    function initGrid() {
      context.SCENE.boundary = new THREE.Geometry();

      var vertices = context.SCENE.boundary.vertices;

      /* Bottom face */
      vertices.push(new THREE.Vector3(-w, -h, -d));
      vertices.push(new THREE.Vector3(w, -h, -d));

      vertices.push(new THREE.Vector3(w, -h, -d));
      vertices.push(new THREE.Vector3(w, -h, d));

      vertices.push(new THREE.Vector3(w, -h, d));
      vertices.push(new THREE.Vector3(-w, -h, d));

      vertices.push(new THREE.Vector3(-w, -h, d));
      vertices.push(new THREE.Vector3(-w, -h, -d));

      /* Sides */
      vertices.push(new THREE.Vector3(-w, -h, -d));
      vertices.push(new THREE.Vector3(-w, h, -d));

      vertices.push(new THREE.Vector3(-w, -h, d));
      vertices.push(new THREE.Vector3(-w, h, d));

      vertices.push(new THREE.Vector3(w, -h, d));
      vertices.push(new THREE.Vector3(w, h, d));

      vertices.push(new THREE.Vector3(w, -h, -d));
      vertices.push(new THREE.Vector3(w, h, -d));

      /* Top face */
      vertices.push(new THREE.Vector3(-w, h, -d));
      vertices.push(new THREE.Vector3(w, h, -d));

      vertices.push(new THREE.Vector3(w, h, -d));
      vertices.push(new THREE.Vector3(w, h, d));

      vertices.push(new THREE.Vector3(w, h, d));
      vertices.push(new THREE.Vector3(-w, h, d));

      vertices.push(new THREE.Vector3(-w, h, d));
      vertices.push(new THREE.Vector3(-w, h, -d));

      var line = new THREE.Line(context.SCENE.boundary,
                                new THREE.LineBasicMaterial({
                                  color: 0x000000,
                                  opacity: 0.1
                                }));
      line.type = THREE.LinePieces;
      context.SCENE.s.add(line);
    }

    function onWindowResize() {
      context.SCENE.camera.left = window.innerWidth / - 2;
      context.SCENE.camera.right = window.innerWidth / 2;
      context.SCENE.camera.top = window.innerHeight / 2;
      context.SCENE.camera.bottom = window.innerHeight / - 2;

      setRendererSize();
      context.SCENE.camera.updateProjectionMatrix();
    }

    var w = config.BOUNDARY.size.x;
    var h = config.BOUNDARY.size.y;
    var d = config.BOUNDARY.size.z;

    context.SCENE.s = new THREE.Scene();

    initCamera();
    initLighting();
    initGrid();

    setRendererSize();
    context.container.appendChild(context.RENDERER.r.domElement);
    window.addEventListener('resize', onWindowResize, false);

    boundaries =
      new THREE.Vector3(w - w * config.BOUNDARY.threshold,
                        h - h * config.BOUNDARY.threshold,
                        d - d * config.BOUNDARY.threshold);

    for (var i = 0; i < config.BOIDS.count; i++)
      createBoid();

    tick(1);
  }

  /* Setup and begin */
  init();

  /* UI COMPONENTS */

  var COHESION_MULTIPLIER = 100000;
  $('#cohesion').text(config.BOIDS.behaviour.cohesion * COHESION_MULTIPLIER);
  $('#cohesion-slider').slider({
    range: 'min',
    min: 0,
    max: 25,
    step: 1,
    value: config.BOIDS.behaviour.cohesion * COHESION_MULTIPLIER,
    slide: function(event, ui) {
      $('#cohesion').text(ui.value);
      config.BOIDS.behaviour.cohesion = ui.value / COHESION_MULTIPLIER;
    }
  });

  var ALIGNMENT_MULTIPLIER = 1000;
  $('#alignment').text(config.BOIDS.behaviour.alignment * ALIGNMENT_MULTIPLIER);
  $('#alignment-slider').slider({
    range: 'min',
    min: 0,
    max: 25,
    step: 1,
    value: config.BOIDS.behaviour.alignment * ALIGNMENT_MULTIPLIER,
    slide: function(event, ui) {
      $('#alignment').text(ui.value);
      config.BOIDS.behaviour.alignment = ui.value / ALIGNMENT_MULTIPLIER;
    }
  });

  var SEPARATION_MULTIPLIER = 0.1;
  var SEPARATION_OFFSET = 40;
  $('#separation').text((config.BOIDS.behaviour.separation.distance - SEPARATION_OFFSET) *
                        SEPARATION_MULTIPLIER);
  $('#separation-slider').slider({
    range: 'min',
    min: 0,
    max: 15,
    step: 1,
    value: (config.BOIDS.behaviour.separation.distance - SEPARATION_OFFSET) *
      SEPARATION_MULTIPLIER,
    slide: function(event, ui) {
      $('#separation').text(ui.value);
      config.BOIDS.behaviour.separation.distance = ui.value / SEPARATION_MULTIPLIER +
        SEPARATION_OFFSET;
    }
  });

  $('#no-of-boids').text(config.BOIDS.count);
  $('#no-of-boids-slider').slider({
    range: 'min',
    min: 1,
    max: 300,
    step: 1,
    value: config.BOIDS.count,
    slide: function(event, ui) {
      var n = ui.value;

      $('#no-of-boids').text(n);

      while (n != config.BOIDS.count) {
        if (n > config.BOIDS.count) {
          createBoid();
          config.BOIDS.count++;
        } else {
          destroyBoid();
          config.BOIDS.count--;
        }
      }
    }
  });

  var SPEED_MULTIPLIER = 10;
  $('#speed').text(config.BOIDS.speed.min * SPEED_MULTIPLIER + ' - ' +
                   config.BOIDS.speed.max * SPEED_MULTIPLIER);
  $('#speed-slider').slider({
    range: true,
    min: 0.5,
    max: 5,
    step: 0.5,
    values: [
      config.BOIDS.speed.min * SPEED_MULTIPLIER,
      config.BOIDS.speed.max * SPEED_MULTIPLIER
    ],
    slide: function(event, ui) {
      $('#speed').text(ui.values[0] + ' - ' + ui.values[1]);
      config.BOIDS.speed.min = ui.values[0] / SPEED_MULTIPLIER;
      config.BOIDS.speed.max = ui.values[1] / SPEED_MULTIPLIER;
    }
  });

  var SIGHT_MULTIPLIER = 0.1;
  $('#sight').text((config.BOIDS.los - config.BOIDS.behaviour.separation.distance) *
                   SIGHT_MULTIPLIER);
  $('#sight-slider').slider({
    range: 'min',
    min: 2,
    max: 50,
    step: 1,
    value: (config.BOIDS.los - config.BOIDS.behaviour.separation.distance) *
      SIGHT_MULTIPLIER,
    slide: function(event, ui) {
      $('#sight').text(ui.value);
      config.BOIDS.los = ui.value / SIGHT_MULTIPLIER +
        config.BOIDS.behaviour.separation.distance;
    }
  });

  $('#first-person').on('switch-change', function (e, data) {
    context.SCENE.firstPerson = data.value;
  });

  $('#reset').click(function() {
    initLighting();

    for (var i = 0; i < config.BOIDS.count; i++)
      destroyBoid();

    for (var i = 0; i < config.BOIDS.count; i++)
      createBoid();
  });

}).call(Boids);
