var Boids = Boids || {};

(function() {
  'use strict';

  /* The configuration */
  var conf = {
    size: new THREE.Vector3(600, 400, 600),
    boids: {
      count: 40,
      size: 10,
      speed: {
        min: 0.15,
        max: 0.35
      },
      los: 160,
      cohesion: 0.00005,
      alignment: 0.010,
      separation: {
        distance: 100,
        rate: 0.007
      }
    },
    boundary: {
      threshold: 0.30,
      rate: 0.00003
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
    this.shadow.y = -conf.size.y;

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
    context.scene.add(this.mesh);
    context.scene.add(this.shadow);
  };

  Boid.prototype.updateMesh = function() {
    /* Position */
    this.mesh.position.copy(this.position);

    /* Heading */
    var nextPos = new THREE.Vector3().copy(this.position).add(this.velocity);

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
    if (this.mesh.position.y > -conf.size.y) {
      this.shadow.material.opacity = 0.1;

      this.shadow.position.copy(this.mesh.position);
      this.shadow.position.y = -conf.size.y;

      this.shadow.rotation.copy(this.mesh.rotation);

      this.shadow.geometry.vertices[4].y = this.mesh.geometry.vertices[4].y;
      this.shadow.geometry.vertices[5].y = this.mesh.geometry.vertices[5].y;
    } else {
      this.shadow.material.opacity = 0;
    }
  };

  /* The context */
  var context = {
    container: document.getElementById('container'),
    camera: null,
    scene: null,
    renderer: new THREE.CanvasRenderer(),
    rendererWidth: 1.0,
    rendererHeight: 0.65,
    boundary: null,
    lights: []
  };

  /* The minimum target frame rate */
  var MIN_FRAMES_PER_SECOND = 30;

  /* A higher physics frame rate will increase the granularity of the simulation
   * at the expense of greater computational costs. */
  var PHYSICS_FRAME_RATE = 50;

  /* Timekeeping (in units of milliseconds) */
  var time = {
    /* The delta time, aka. the size of the chunk of time to process for each
     * update of the simulation. */
    dt: 1000 / PHYSICS_FRAME_RATE,
    /* The maximum amount of time to process for a given iteration of the render
     * loop. The time is capped at this value, meaning that the simulation will
     * begin to slow down if the tick time exceeds this value.  */
    maxTickTime: 1000 / MIN_FRAMES_PER_SECOND,
    /* The current time */
    current: new Date().getTime(),
    /* The accumulated error between the frequency of ticks and render
     * updates */
    accumulator: 0,
  };

  /* The boids */
  var boids = [];

  var forces = {
    cohesion: 0,
    boundary: 0
  };

  var speedLimits = {
    min: 0,
    max: 0
  };

  var boundaries;

  function createBoid() {
    boids.push(new Boid());
  }

  function destroyBoid() {
    var b = boids.pop();

    context.scene.remove(b.mesh);
    context.scene.remove(b.shadow);
  }

  /* Update function */
  function tick(timestamp) {

    function update(dt) {

      function updateBoid(index) {

        function enforceSpeedLimits(v) {
          var speed = v.length();

          b.speed = speed;

          if (speed > speedLimits.max) {

            /* Maximum speed */
            b.speed = speedLimits.max;

            v.multiplyScalar(speedLimits.max / speed);
          } else if (speed < speedLimits.min) {

            /* Minimum speed */
            b.speed = speedLimits.min;
            speed = Math.max(speed, 0.0001);

            v.multiplyScalar(speedLimits.min / speed);
          }
        }

        var b = boids[index];
        var dv = new THREE.Vector3(0, 0, 0); // Change in velocity
        var centerOfMass = new THREE.Vector3(0, 0, 0);
        var velocityAvg = new THREE.Vector3(0, 0, 0);
        var swarmSize = 0;

        for (var i = 0; i < conf.boids.count; i++) {
          if (i !== index) {
            var otherBoid = boids[i];

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
            if (distance < conf.boids.separation.distance) {
              dv.sub(new THREE.Vector3()
                     .subVectors(otherBoid.position, b.position)
                     .multiplyScalar(conf.boids.separation.rate));
            }

            /* We total up the velocity and positions of any particles that are
             * within the range of visibility of the current particle:
             */
            if (distance < conf.boids.los) {
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
               .multiplyScalar(forces.cohesion));

        /*
         * FLOCK ALIGNMENT
         *
         * Boids try to match velocity with other boids nearby, this creates a
         * pattern of cohesive behaviour, with the flock moving in unison:
         */
        dv.add(new THREE.Vector3()
               .subVectors(velocityAvg, b.velocity)
               .multiplyScalar(conf.boids.alignment));

        /*
         * BOUNDARY AVOIDANCE
         *
         * Boids avoid boundaries by being negatively accelerated away from
         * them when the distance to the boundary is less than a known
         * threshold:
         */
        if (b.position.x < -boundaries.x)
          dv.x += (-boundaries.x - b.position.x) * forces.boundary * b.speed;
        else if (b.position.x > boundaries.x)
          dv.x += (boundaries.x - b.position.x) * forces.boundary * b.speed;

        if (b.position.y < -boundaries.y)
          dv.y += (-boundaries.y - b.position.y) * forces.boundary * b.speed;
        else if (b.position.y > boundaries.y)
          dv.y += (boundaries.y - b.position.y) * forces.boundary * b.speed;

        if (b.position.z < -boundaries.z)
          dv.z += (-boundaries.z - b.position.z) * forces.boundary * b.speed;
        else if (b.position.z > boundaries.z)
          dv.z += (boundaries.z - b.position.z) * forces.boundary * b.speed;

        /* Apply the velocity change */
        b.velocity.add(dv);

        /* Control the rate of boids movement */
        enforceSpeedLimits(b.velocity);

        /* Update position */
        b.position.add(b.velocity);

        b.updateMesh();
      }

      /* Update forces */
      forces.cohesion = dt * conf.boids.cohesion;
      forces.boundary = dt * conf.boundary.rate;

      /* Update speed limits */
      speedLimits.min = dt * conf.boids.speed.min;
      speedLimits.max = dt * conf.boids.speed.max;

      for (var i = 0; i < conf.boids.count; i++)
        updateBoid(i);
    }

    function render() {
      var timer = Date.now() * 0.00005;
      var x = Math.cos(timer) * conf.size.x * 1.75;
      var z = Math.sin(timer) * conf.size.z * 1.75;

      context.camera.position.x = x
      context.camera.position.z = z;

      context.camera.lookAt(context.cameraTarget);
      context.renderer.render(context.scene, context.camera);
    }

    /* Update the clocks */
    var newTime = new Date().getTime();
    var tickTime = newTime - time.current;

    /* Enforce a maximum frame time to prevent the "spiral of death" when
     * operating under heavy load */
    if (tickTime > time.maxTickTime)
      tickTime = time.maxTickTime;

    time.current = newTime;
    time.accumulator += tickTime;

    /* Update the simulation state as required */
    while (time.accumulator >= time.dt) {
      update(time.dt);
      time.accumulator -= time.dt;
    }

    /* Render the new state */
    render();

    /* Request a new tick */
    requestAnimationFrame(tick);
  }

  function initLighting() {

    var lights = context.lights;
    var scene = context.scene;

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
    var w = container.offsetWidth * context.rendererWidth;
    var h = window.innerHeight * context.rendererHeight;

    context.renderer.setSize(w, h);
  }

  /* Initialisation function */
  function init() {

    function initCamera() {
      /* Camera configuration */
      var fov = 60;
      var aspect = window.innerWidth / window.innerHeight;
      var zNear = 1;
      var zFar = 10000;

      context.camera = new THREE.PerspectiveCamera(fov, aspect, zNear, zFar);
      context.camera.position.y = conf.size.y * 0.4;
      context.cameraTarget = new THREE.Vector3(context.scene.position.x,
                                               context.scene.position.y,
                                               context.scene.position.z);
    }

    function initGrid() {
      context.boundary = new THREE.Geometry();

      /* Bottom face */
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       -conf.size.y,
                                                       -conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       -conf.size.y,
                                                       -conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       -conf.size.y,
                                                       -conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       -conf.size.y,
                                                       conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       -conf.size.y,
                                                       conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       -conf.size.y,
                                                       conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       -conf.size.y,
                                                       conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       -conf.size.y,
                                                       -conf.size.z));

      /* Sides */
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       -conf.size.y,
                                                       -conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       conf.size.y,
                                                       -conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       -conf.size.y,
                                                       conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       conf.size.y,
                                                       conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       -conf.size.y,
                                                       conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       conf.size.y,
                                                       conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       -conf.size.y,
                                                       -conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       conf.size.y,
                                                       -conf.size.z));

      /* Top face */
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       conf.size.y,
                                                       -conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       conf.size.y,
                                                       -conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       conf.size.y,
                                                       -conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       conf.size.y,
                                                       conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                       conf.size.y,
                                                       conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       conf.size.y,
                                                       conf.size.z));

      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       conf.size.y,
                                                       conf.size.z));
      context.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                       conf.size.y,
                                                       -conf.size.z));

      var line = new THREE.Line(context.boundary,
                                new THREE.LineBasicMaterial({
                                  color: 0x000000,
                                  opacity: 0.1
                                }));
      line.type = THREE.LinePieces;
      context.scene.add(line);
    }

    function onWindowResize() {
      context.camera.left = window.innerWidth / - 2;
      context.camera.right = window.innerWidth / 2;
      context.camera.top = window.innerHeight / 2;
      context.camera.bottom = window.innerHeight / - 2;

      setRendererSize();
      context.camera.updateProjectionMatrix();
    }

    context.scene = new THREE.Scene();

    initCamera();
    initLighting();
    initGrid();

    setRendererSize();
    context.container.appendChild(context.renderer.domElement);
    window.addEventListener('resize', onWindowResize, false);

    boundaries =
      new THREE.Vector3(conf.size.x - conf.size.x * conf.boundary.threshold,
                        conf.size.y - conf.size.y * conf.boundary.threshold,
                        conf.size.z - conf.size.z * conf.boundary.threshold);

    for (var i = 0; i < conf.boids.count; i++)
      createBoid();

    tick(1);
  }

  /* Setup and begin */
  init();

  /* UI COMPONENTS */

  var COHESION_MULTIPLIER = 100000;
  $('#cohesion').text(conf.boids.cohesion * COHESION_MULTIPLIER);
  $('#cohesion-slider').slider({
    range: 'min',
    min: 0,
    max: 25,
    step: 1,
    value: conf.boids.cohesion * COHESION_MULTIPLIER,
    slide: function(event, ui) {
      $('#cohesion').text(ui.value);
      conf.boids.cohesion = ui.value / COHESION_MULTIPLIER;
    }
  });

  var ALIGNMENT_MULTIPLIER = 1000;
  $('#alignment').text(conf.boids.alignment * ALIGNMENT_MULTIPLIER);
  $('#alignment-slider').slider({
    range: 'min',
    min: 0,
    max: 25,
    step: 1,
    value: conf.boids.alignment * ALIGNMENT_MULTIPLIER,
    slide: function(event, ui) {
      $('#alignment').text(ui.value);
      conf.boids.alignment = ui.value / ALIGNMENT_MULTIPLIER;
    }
  });

  var SEPARATION_MULTIPLIER = 0.1;
  $('#separation').text(conf.boids.separation.distance * SEPARATION_MULTIPLIER);
  $('#separation-slider').slider({
    range: 'min',
    min: 0,
    max: 25,
    step: 1,
    value: conf.boids.separation.distance * SEPARATION_MULTIPLIER,
    slide: function(event, ui) {
      $('#separation').text(ui.value);
      conf.boids.separation.distance = ui.value / SEPARATION_MULTIPLIER;
    }
  });

  $('#no-of-boids').text(conf.boids.count);
  $('#no-of-boids-slider').slider({
    range: 'min',
    min: 1,
    max: 300,
    step: 1,
    value: conf.boids.count,
    slide: function(event, ui) {
      var n = ui.value;

      $('#no-of-boids').text(n);

      while (n != conf.boids.count) {
        if (n > conf.boids.count) {
          createBoid();
          conf.boids.count++;
        } else {
          destroyBoid();
          conf.boids.count--;
        }
      }
    }
  });

  var SPEED_MULTIPLIER = 10;
  $('#speed').text(conf.boids.speed.min * SPEED_MULTIPLIER + ' - ' +
                   conf.boids.speed.max * SPEED_MULTIPLIER);
  $('#speed-slider').slider({
    range: true,
    min: 0.5,
    max: 5,
    step: 0.5,
    values: [
      conf.boids.speed.min * SPEED_MULTIPLIER,
      conf.boids.speed.max * SPEED_MULTIPLIER
    ],
    slide: function(event, ui) {
      $('#speed').text(ui.values[0] + ' - ' + ui.values[1]);
      conf.boids.speed.min = ui.values[0] / SPEED_MULTIPLIER;
      conf.boids.speed.max = ui.values[1] / SPEED_MULTIPLIER;
    }
  });

  var SIGHT_MULTIPLIER = 0.1;
  $('#sight').text((conf.boids.los - conf.boids.separation.distance) *
                   SIGHT_MULTIPLIER);
  $('#sight-slider').slider({
    range: 'min',
    min: 2,
    max: 50,
    step: 2,
    value: (conf.boids.los - conf.boids.separation.distance) *
      SIGHT_MULTIPLIER,
    slide: function(event, ui) {
      $('#sight').text(ui.value);
      conf.boids.los = ui.value / SIGHT_MULTIPLIER +
        conf.boids.separation.distance;
    }
  });

  $('#width').text(Math.round(context.rendererWidth * 100) + '%');
  $('#width-slider').slider({
    range: 'min',
    min: 0.50,
    max: 1.0,
    step: 0.01,
    value: context.rendererWidth,
    slide: function(event, ui) {
      $('#width').text(Math.round(ui.value * 100) + '%');
      context.rendererWidth = ui.value;
      setRendererSize();
    }
  });

  $('#height').text(Math.round(context.rendererHeight * 100) + '%');
  $('#height-slider').slider({
    range: 'min',
    min: 0.50,
    max: 1.0,
    step: 0.01,
    value: context.rendererHeight,
    slide: function(event, ui) {
      $('#height').text(Math.round(ui.value * 100) + '%');
      context.rendererHeight = ui.value;
      setRendererSize();
    }
  });

  $('#reset').click(function() {
    initLighting();

    for (var i = 0; i < conf.boids.count; i++)
      destroyBoid();

    for (var i = 0; i < conf.boids.count; i++)
      createBoid();
  });

}).call(Boids);
