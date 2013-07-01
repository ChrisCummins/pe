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

/* Boids */
var Boids = Boids || {};

(function() {
  'use strict';

  /* The drawing context */
  var ctx = {
    container: document.getElementById('container'),
    camera: null,
    scene: null,
    renderer: new THREE.CanvasRenderer(),
    rendererSize: 0.65,
    boundary: null,
    lights: []
  };

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
      cohesion: 0.00002,
      alignment: 0.004,
      separation: {
        distance: 60,
        rate: 0.0010
      }
    },
    boundary: {
      threshold: 0.30,
      rate: 0.0020
    }
  };

  /* Boid behaviour */
  function Boid() {
    var geometry = new Bird();

    this.position = [];
    this.velocity = [];
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

    for (var j = 0; j < 3; j++) {
      /* This magic variable determines the maximum starting speed. */
      var INITIAL_SPEED_MULTIPLIER = 2;

      this.position.push(Math.random() * boundaries[j] * 2 - boundaries[j]),
      this.velocity.push(2 * INITIAL_SPEED_MULTIPLIER * Math.random() -
                         INITIAL_SPEED_MULTIPLIER);
    }

    this.updateMesh();
    ctx.scene.add(this.mesh);
    ctx.scene.add(this.shadow);
  };

  Boid.prototype.updateMesh = function() {
    /* Position */
    this.mesh.position.x = this.position[0];
    this.mesh.position.y = this.position[1];
    this.mesh.position.z = this.position[2];

    /* Heading */
    var nextPos = new THREE.Vector3(this.position[0] + this.velocity[0],
                                    this.position[1] + this.velocity[1],
                                    this.position[2] + this.velocity[2]);
    var rotY = Math.atan2(-this.velocity[2], this.velocity[0]);
    var rotZ = Math.asin(this.velocity[1] / this.speed);

    /* FIXME: what a total hack (!) */
    if (isNaN(rotZ))
      rotZ = 0;

    this.mesh.rotation.y = rotY;
    this.mesh.rotation.z = rotZ;

    /* Wing flapping */
    this.phase = (this.phase + (Math.max(0, this.mesh.rotation.z) + 0.1)) % 62.83;

    var wingY = Math.sin(this.phase) * 10;

    this.mesh.geometry.vertices[4].y = wingY;
    this.mesh.geometry.vertices[5].y = wingY;

    /* The Shadow */
    this.shadow.position.x = this.mesh.position.x;
    this.shadow.position.y = -conf.size.y;
    this.shadow.position.z = this.mesh.position.z;

    this.shadow.rotation.y = this.mesh.rotation.y;
    this.shadow.rotation.z = this.mesh.rotation.z;

    this.shadow.geometry.vertices[4].y = this.mesh.geometry.vertices[4].y;
    this.shadow.geometry.vertices[5].y = this.mesh.geometry.vertices[5].y;
  };

  /* The boids */
  var boids = [];

  var lastUpdateTime = 0;

  var forces = {
    cohesion: 0,
    boundary: 0
  };

  var speedLimits = {
    min: 0,
    max: 0
  };

  var boundaries = [0, 0, 0];

  function createBoid() {
    boids.push(new Boid());
  }

  function destroyBoid() {
    var b = boids.pop();

    ctx.scene.remove(b.mesh);
    ctx.scene.remove(b.shadow);
  }

  function createResources() {

    function createGrid() {
      ctx.boundary = new THREE.Geometry();

      /* Bottom face */
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   -conf.size.y,
                                                   -conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   -conf.size.y,
                                                   -conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   -conf.size.y,
                                                   -conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   -conf.size.y,
                                                   conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   -conf.size.y,
                                                   conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   -conf.size.y,
                                                   conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   -conf.size.y,
                                                   conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   -conf.size.y,
                                                   -conf.size.z));

      /* Sides */
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   -conf.size.y,
                                                   -conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   conf.size.y,
                                                   -conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   -conf.size.y,
                                                   conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   conf.size.y,
                                                   conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   -conf.size.y,
                                                   conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   conf.size.y,
                                                   conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   -conf.size.y,
                                                   -conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   conf.size.y,
                                                   -conf.size.z));

      /* Top face */
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   conf.size.y,
                                                   -conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   conf.size.y,
                                                   -conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   conf.size.y,
                                                   -conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   conf.size.y,
                                                   conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(conf.size.x,
                                                   conf.size.y,
                                                   conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   conf.size.y,
                                                   conf.size.z));

      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   conf.size.y,
                                                   conf.size.z));
      ctx.boundary.vertices.push(new THREE.Vector3(-conf.size.x,
                                                   conf.size.y,
                                                   -conf.size.z));

      var line = new THREE.Line(ctx.boundary,
                                new THREE.LineBasicMaterial({
                                  color: 0x000000,
                                  opacity: 0.1
                                }));
      line.type = THREE.LinePieces;
      ctx.scene.add(line);
    }

    boundaries = [
      conf.size.x - conf.size.x * conf.boundary.threshold,
      conf.size.y - conf.size.y * conf.boundary.threshold,
      conf.size.z - conf.size.z * conf.boundary.threshold
    ];

    createGrid();

    for (var i = 0; i < conf.boids.count; i++)
      createBoid();
  }

  /* Update function */
  function tick(timestamp) {

    function update(timestamp) {

      function updateBoid(index) {

        function enforceSpeedLimits(v) {
          var mag = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
          var newMag = mag;

          if (mag > speedLimits.max) {

            /* Maximum speed */
            newMag = speedLimits.max;

            for (var i = 0; i < 3; i++)
              v[i] = (v[i] / mag) * speedLimits.max;
          } else if (mag < speedLimits.min) {

            /* Minimum speed */
            newMag = speedLimits.min;
            mag = Math.max(mag, 0.0001);

            for (var i = 0; i < 3; i++)
              v[i] = (v[i] / mag) * speedLimits.min;
          }

          b.speed = newMag;
        }

        var b = boids[index];
        var dv = [0, 0, 0]; // Change in velocity
        var centerOfMass = [0, 0, 0];
        var velocityAvg = [0, 0, 0];
        var swarmSize = 0;

        for (var i = 0; i < conf.boids.count; i++) {
          if (i !== index) {
            var otherBoid = boids[i];

            var dp = [
              b.position[0] - otherBoid.position[0],
              b.position[1] - otherBoid.position[1],
              b.position[2] - otherBoid.position[2]
            ];

            /* Get the distance between the other Boid and this Boid */
            var distance = Math.sqrt(dp[0] * dp[0] +
                                     dp[1] * dp[1] +
                                     dp[2] * dp[2]);

            /*
             * COLLISION AVOIDANCE
             *
             * Boids try to keep a small distance away from other boids to
             * prevent them bumping into each other and reduce the density of
             * the flock:
             */
            if (distance < conf.boids.separation.distance) {
              for (var j = 0; j < 3; j++) {
                dv[j] -= (otherBoid.position[j] - b.position[j]) *
                    conf.boids.separation.rate;
              }
            }

            /* We total up the velocity and positions of any particles that are
             * within the range of visibility of the current particle:
             */
            if (distance < conf.boids.los) {

              for (var j = 0; j < 3; j++) {
                centerOfMass[j] += otherBoid.position[j];
                velocityAvg[j] += otherBoid.velocity[j];
              }

              swarmSize++;
            }
          }
        }

        /* We must always have a flock to compare against, even
         * if a Boid is on it's own: */
        if (swarmSize < 1) {
          for (var j = 0; j < 3; j++)
            centerOfMass[j] = b.position[j];

          swarmSize = 1;
        }

        for (var i = 0; i < 3; i++) {
          centerOfMass[i] /= swarmSize;
          velocityAvg[i] /= swarmSize;

          /*
           * PARTICLE COHESION
           *
           * Boids try to fly towards the centre of mass of neighbouring
           * boids. We do this by first calculating a 'center of mass' for the
           * flock, and moving the boid by an amount proportional to it's
           * distance from that center:
           */
          dv[i] += (centerOfMass[i] - b.position[i]) * forces.cohesion;

          /*
           * FLOCK ALIGNMENT
           *
           * Boids try to match velocity with other boids nearby, this creates a
           * pattern of cohesive behaviour, with the flock moving in unison:
           */
          dv[i] += (velocityAvg[i] - b.velocity[1]) * conf.boids.alignment;

          /*
           * BOUNDARY AVOIDANCE
           *
           * Boids avoid boundaries by being negatively accelerated away from
           * them when the distance to the boundary is less than a known
           * threshold:
           */
          if (b.position[i] < -boundaries[i])
            dv[i] += forces.boundary * b.speed;
          else if (b.position[i] > boundaries[i])
            dv[i] -= forces.boundary * b.speed;
        }

        /* Apply the velocity change */
        for (var i = 0; i < 3; i++)
          b.velocity[i] += dv[i];

        /* Control the rate of boids movement */
        enforceSpeedLimits(b.velocity);

        /* Update position */
        b.position[0] += b.velocity[0];
        b.position[1] += b.velocity[1];
        b.position[2] += b.velocity[2];

        b.updateMesh();
      }

      var tickTime = timestamp - lastUpdateTime;
      lastUpdateTime = timestamp;

      /* Update forces */
      forces.cohesion = tickTime * conf.boids.cohesion;
      forces.boundary = tickTime * conf.boundary.rate;

      /* Update speed limits */
      speedLimits.min = tickTime * conf.boids.speed.min;
      speedLimits.max = tickTime * conf.boids.speed.max;

      for (var i = 0; i < conf.boids.count; i++)
        updateBoid(i);
    }

    function render() {
      var timer = Date.now() * 0.00005;

      ctx.camera.position.x = Math.cos(timer) * conf.size.x * 1.75;
      ctx.camera.position.z = Math.sin(timer) * conf.size.z * 1.75;

      ctx.camera.lookAt(ctx.cameraTarget);

      ctx.renderer.render(ctx.scene, ctx.camera);
    }

    update(timestamp);
    render();
    requestAnimationFrame(tick);
  }

  function initLighting() {

    if (ctx.lights.length > 0) {
      /* Clear lights */
      for (var i = ctx.lights.length - 1; i >= 0; i--)
        ctx.scene.remove(ctx.lights.pop());
    }

    var ambientLight = new THREE.AmbientLight(0x111133);
    ctx.scene.add(ambientLight);
    ctx.lights.push(ambientLight);

    var light = new THREE.DirectionalLight(0x7a2338);
    light.position.set(Math.random() - 0.5,
                       Math.random() - 0.5,
                       Math.random() - 0.5);
    light.position.normalize();
    ctx.scene.add(light);

    ctx.lights.push(light);

    var light = new THREE.DirectionalLight(0x21d592);
    light.position.set(Math.random() - 0.5,
                       Math.random() - 0.5,
                       Math.random() - 0.5);
    light.position.normalize();
    ctx.scene.add(light);

    ctx.lights.push(light);
  }

  function setRendererSize() {
    ctx.renderer.setSize(container.offsetWidth,
                         window.innerHeight * ctx.rendererSize);
  }

  /* Initialisation function */
  function init() {

    function initCamera() {

      ctx.camera = new THREE.PerspectiveCamera(60,
                                               window.innerWidth /
                                               window.innerHeight,
                                               1, 10000);

      ctx.camera.position.y = conf.size.y * 0.4;

      ctx.cameraTarget = new THREE.Vector3(ctx.scene.position.x,
                                           ctx.scene.position.y,
                                           ctx.scene.position.z);
    }

    function onWindowResize() {
      ctx.camera.left = window.innerWidth / - 2;
      ctx.camera.right = window.innerWidth / 2;
      ctx.camera.top = window.innerHeight / 2;
      ctx.camera.bottom = window.innerHeight / - 2;

      setRendererSize();
      ctx.camera.updateProjectionMatrix();
    }

    ctx.scene = new THREE.Scene();

    initCamera();
    initLighting();

    setRendererSize();
    ctx.container.appendChild(ctx.renderer.domElement);
    window.addEventListener('resize', onWindowResize, false);
  }

  /* Setup and begin */
  init();
  createResources();
  tick(1);

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
    max: 200,
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

  $('#size').text(Math.round(ctx.rendererSize * 100) + '%');
  $('#size-slider').slider({
    range: 'min',
    min: 0.50,
    max: 1.0,
    step: 0.01,
    value: ctx.rendererSize,
    slide: function(event, ui) {
      $('#size').text(Math.round(ui.value * 100) + '%');
      ctx.rendererSize = ui.value;
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
