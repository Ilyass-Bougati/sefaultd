import http from 'k6/http';
import { check } from 'k6';
import { Counter } from 'k6/metrics';

const timeouts = new Counter('hangs');

export const options = {
  scenarios: {
    // Steady concurrency — baseline correctness under load.
    steady: {
      executor: 'constant-vus',
      vus: 250,
      duration: '150s',
    },
    // Burst — many connections arriving in the same instant.
    // This is what reproduces accept-loop and shared-buffer races.
    burst: {
      executor: 'constant-arrival-rate',
      rate: 200,
      timeUnit: '1s',
      duration: '120s',
      preAllocatedVUs: 100,
      maxVUs: 500,
      startTime: '30s',
    },
  },
  thresholds: {
    hangs: ['count==0'],
    http_req_failed: ['rate<0.34'],
    http_req_duration: ['p(99)<1000'],
  },
};

const BASE = __ENV.BASE || 'https://ilyass-bougati.dev';

// Short timeout: a hang must surface as a failed request, not a stalled VU.
const PARAMS = { timeout: '3s' };

export default function () {
  // Alternate 200 and 404 paths — the 404 branch is the one under suspicion.
  const hit404 = __ITER % 3 === 0;
  const path = hit404 ? '/index.htmll' : '/index.html';

  const res = http.get(`${BASE}${path}`, PARAMS);

  if (res.error_code === 1050 || res.status === 0) {
    timeouts.add(1);
    return;
  }

  check(res, {
    'status correct': (r) => r.status === (hit404 ? 404 : 200),
    'body non-empty': (r) => r.body && r.body.length > 0,
    // The critical one: declared length must equal received length.
    'length matches body': (r) => {
      const declared = parseInt(r.headers['Content-Length'], 10);
      return !isNaN(declared) && declared === r.body.length;
    },
    'content-type present': (r) => !!r.headers['Content-Type'],
  });
}
