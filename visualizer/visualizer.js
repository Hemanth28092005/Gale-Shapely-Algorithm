/* Interactive Gale-Shapley Visualizer Engine */

const PRESETS = {
  spec: {
    num_residents: 5,
    num_hospitals: 3,
    residents: [
      { id: 0, name: "R0", prefs: [1, 0, 2] },
      { id: 1, name: "R1", prefs: [0, 2] },
      { id: 2, name: "R2", prefs: [0, 1, 2] },
      { id: 3, name: "R3", prefs: [1, 2] },
      { id: 4, name: "R4", prefs: [0, 1] }
    ],
    hospitals: [
      { id: 0, name: "H0", capacity: 2, prefs: [1, 0, 2, 3, 4] },
      { id: 1, name: "H1", capacity: 2, prefs: [0, 2, 3, 4, 1] },
      { id: 2, name: "H2", capacity: 1, prefs: [3, 1, 0, 2, 4] }
    ],
    steps: [
      { step: 1, type: "PROPOSAL", resident: 4, hospital: 0, bumped: -1 },
      { step: 1, type: "ACCEPT_VACANCY", resident: 4, hospital: 0, bumped: -1 },
      { step: 2, type: "PROPOSAL", resident: 3, hospital: 1, bumped: -1 },
      { step: 2, type: "ACCEPT_VACANCY", resident: 3, hospital: 1, bumped: -1 },
      { step: 3, type: "PROPOSAL", resident: 2, hospital: 0, bumped: -1 },
      { step: 3, type: "ACCEPT_VACANCY", resident: 2, hospital: 0, bumped: -1 },
      { step: 4, type: "PROPOSAL", resident: 1, hospital: 0, bumped: -1 },
      { step: 4, type: "ACCEPT_BUMP", resident: 1, hospital: 0, bumped: 4 },
      { step: 5, type: "PROPOSAL", resident: 4, hospital: 1, bumped: -1 },
      { step: 5, type: "ACCEPT_VACANCY", resident: 4, hospital: 1, bumped: -1 },
      { step: 6, type: "PROPOSAL", resident: 0, hospital: 1, bumped: -1 },
      { step: 6, type: "ACCEPT_BUMP", resident: 0, hospital: 1, bumped: 4 }
    ],
    metrics: { proposals: 6, rejections: 0, bumps: 2, stable: true }
  },
  handbuilt: {
    num_residents: 3,
    num_hospitals: 2,
    residents: [
      { id: 0, name: "R0", prefs: [0, 1] },
      { id: 1, name: "R1", prefs: [0, 1] },
      { id: 2, name: "R2", prefs: [1, 0] }
    ],
    hospitals: [
      { id: 0, name: "H0", capacity: 1, prefs: [0, 1, 2] },
      { id: 1, name: "H1", capacity: 1, prefs: [1, 0, 2] }
    ],
    steps: [
      { step: 1, type: "PROPOSAL", resident: 2, hospital: 1, bumped: -1 },
      { step: 1, type: "ACCEPT_VACANCY", resident: 2, hospital: 1, bumped: -1 },
      { step: 2, type: "PROPOSAL", resident: 1, hospital: 0, bumped: -1 },
      { step: 2, type: "ACCEPT_VACANCY", resident: 1, hospital: 0, bumped: -1 },
      { step: 3, type: "PROPOSAL", resident: 0, hospital: 0, bumped: -1 },
      { step: 3, type: "ACCEPT_BUMP", resident: 0, hospital: 0, bumped: 1 },
      { step: 4, type: "PROPOSAL", resident: 1, hospital: 1, bumped: -1 },
      { step: 4, type: "ACCEPT_BUMP", resident: 1, hospital: 1, bumped: 2 },
      { step: 5, type: "PROPOSAL", resident: 2, hospital: 0, bumped: -1 },
      { step: 5, type: "REJECT_FULL", resident: 2, hospital: 0, bumped: -1 }
    ],
    metrics: { proposals: 5, rejections: 1, bumps: 2, stable: true }
  },
  cascading: {
    num_residents: 5,
    num_hospitals: 2,
    residents: [
      { id: 0, name: "R0", prefs: [0, 1] },
      { id: 1, name: "R1", prefs: [0, 1] },
      { id: 2, name: "R2", prefs: [0, 1] },
      { id: 3, name: "R3", prefs: [0, 1] },
      { id: 4, name: "R4", prefs: [1, 0] }
    ],
    hospitals: [
      { id: 0, name: "H0", capacity: 2, prefs: [0, 1, 2, 3, 4] },
      { id: 1, name: "H1", capacity: 2, prefs: [2, 3, 4, 0, 1] }
    ],
    steps: [
      { step: 1, type: "PROPOSAL", resident: 4, hospital: 1, bumped: -1 },
      { step: 1, type: "ACCEPT_VACANCY", resident: 4, hospital: 1, bumped: -1 },
      { step: 2, type: "PROPOSAL", resident: 3, hospital: 0, bumped: -1 },
      { step: 2, type: "ACCEPT_VACANCY", resident: 3, hospital: 0, bumped: -1 },
      { step: 3, type: "PROPOSAL", resident: 2, hospital: 0, bumped: -1 },
      { step: 3, type: "ACCEPT_VACANCY", resident: 2, hospital: 0, bumped: -1 },
      { step: 4, type: "PROPOSAL", resident: 1, hospital: 0, bumped: -1 },
      { step: 4, type: "ACCEPT_BUMP", resident: 1, hospital: 0, bumped: 3 },
      { step: 5, type: "PROPOSAL", resident: 3, hospital: 1, bumped: -1 },
      { step: 5, type: "ACCEPT_VACANCY", resident: 3, hospital: 1, bumped: -1 },
      { step: 6, type: "PROPOSAL", resident: 0, hospital: 0, bumped: -1 },
      { step: 6, type: "ACCEPT_BUMP", resident: 0, hospital: 0, bumped: 2 },
      { step: 7, type: "PROPOSAL", resident: 2, hospital: 1, bumped: -1 },
      { step: 7, type: "ACCEPT_BUMP", resident: 2, hospital: 1, bumped: 4 }
    ],
    metrics: { proposals: 7, rejections: 0, bumps: 3, stable: true }
  }
};

let currentDataset = PRESETS.spec;
let currentStepIndex = 0;
let isPlaying = false;
let playTimer = null;
let playbackSpeed = 1.0;

// State snapshot per step index
let stateHistory = [];

function initDataset(data) {
  currentDataset = data;
  currentStepIndex = 0;
  isPlaying = false;
  if (playTimer) clearInterval(playTimer);

  document.getElementById("btn-play").innerHTML = "&#x25B6; Play";
  document.getElementById("resident-count-badge").innerText = `${data.num_residents} Residents`;
  document.getElementById("hospital-count-badge").innerText = `${data.num_hospitals} Hospitals`;

  const stabilityBadge = document.getElementById("stability-badge");
  if (data.metrics && data.metrics.stable) {
    stabilityBadge.className = "status-badge stable";
    stabilityBadge.innerHTML = '<span class="dot"></span> STABLE MATCHING';
  } else {
    stabilityBadge.className = "status-badge";
    stabilityBadge.innerHTML = '<span class="dot" style="background:#ef4444"></span> VERIFYING';
  }

  buildInitialStateHistory();
  renderState(0);
}

function buildInitialStateHistory() {
  stateHistory = [];

  // Step 0: initial state
  let state = {
    stepIndex: 0,
    residents: currentDataset.residents.map(r => ({
      ...r,
      match: -1,
      status: "unmatched", // unmatched, proposing, matched, bumped, rejected
      activeHospital: -1
    })),
    hospitals: currentDataset.hospitals.map(h => ({
      ...h,
      assigned: []
    })),
    metrics: { proposals: 0, bumps: 0, rejections: 0 },
    actionText: "Ready to begin matching.",
    logEntries: []
  };

  stateHistory.push(JSON.parse(JSON.stringify(state)));

  for (let i = 0; i < currentDataset.steps.length; i++) {
    const step = currentDataset.steps[i];
    const prev = stateHistory[stateHistory.length - 1];
    let next = JSON.parse(JSON.stringify(prev));
    next.stepIndex = i + 1;

    // Reset temporary statuses
    next.residents.forEach(r => {
      if (r.status === "proposing" || r.status === "bumped") {
        r.status = r.match !== -1 ? "matched" : "unmatched";
      }
      r.activeHospital = -1;
    });

    const rObj = next.residents[step.resident];
    const hObj = next.hospitals[step.hospital];

    rObj.activeHospital = step.hospital;

    let logText = "";
    let logClass = "";

    switch (step.type) {
      case "PROPOSAL":
        next.metrics.proposals++;
        rObj.status = "proposing";
        next.actionText = `<span class="action-highlight-r">${rObj.name}</span> proposes to <span class="action-highlight-h">${hObj.name}</span>`;
        logText = `[Step ${step.step}] ${rObj.name} proposes to ${hObj.name}`;
        logClass = "proposal";
        break;

      case "ACCEPT_VACANCY":
        rObj.status = "matched";
        rObj.match = step.hospital;
        hObj.assigned.push(step.resident);
        next.actionText = `<span class="action-highlight-h">${hObj.name}</span> provisionally accepts <span class="action-highlight-r">${rObj.name}</span> (seat ${hObj.assigned.length}/${hObj.capacity})`;
        logText = `       ↳ ${hObj.name} provisionally accepts ${rObj.name} (vacancy)`;
        logClass = "accept";
        break;

      case "ACCEPT_BUMP":
        next.metrics.bumps++;
        rObj.status = "matched";
        rObj.match = step.hospital;

        // Evict bumped resident
        const bumpedIdx = hObj.assigned.indexOf(step.bumped);
        if (bumpedIdx !== -1) {
          hObj.assigned[bumpedIdx] = step.resident;
        } else {
          hObj.assigned.push(step.resident);
        }

        const bumpedObj = next.residents[step.bumped];
        bumpedObj.match = -1;
        bumpedObj.status = "bumped";

        next.actionText = `<span class="action-highlight-h">${hObj.name}</span> bumps <span class="action-highlight-bump">${bumpedObj.name}</span> for <span class="action-highlight-r">${rObj.name}</span>!`;
        logText = `       ↳ ${hObj.name} BUMPS ${bumpedObj.name} for ${rObj.name}`;
        logClass = "bump";
        break;

      case "REJECT_FULL":
      case "REJECT_UNACCEPTABLE":
      case "REJECT_ZERO_CAPACITY":
        next.metrics.rejections++;
        rObj.status = "rejected";
        next.actionText = `<span class="action-highlight-h">${hObj.name}</span> rejects <span class="action-highlight-r">${rObj.name}</span>`;
        logText = `       ↳ ${hObj.name} REJECTS ${rObj.name}`;
        logClass = "reject";
        break;
    }

    next.logEntries.push({ text: logText, type: logClass });
    stateHistory.push(next);
  }
}

function renderState(stepIdx) {
  if (stepIdx < 0 || stepIdx >= stateHistory.length) return;
  currentStepIndex = stepIdx;
  const state = stateHistory[stepIdx];

  // Update counters
  document.getElementById("metric-step").innerText = `${stepIdx} / ${stateHistory.length - 1}`;
  document.getElementById("metric-proposals").innerText = state.metrics.proposals;
  document.getElementById("metric-bumps").innerText = state.metrics.bumps;
  document.getElementById("metric-rejections").innerText = state.metrics.rejections;

  const matchedCount = state.residents.filter(r => r.match !== -1).length;
  document.getElementById("metric-matched").innerText = `${matchedCount} / ${state.residents.length}`;

  // Current Action box
  document.getElementById("current-action-display").innerHTML = `<div class="action-text">${state.actionText}</div>`;

  // Render Residents
  const resContainer = document.getElementById("residents-container");
  resContainer.innerHTML = "";
  state.residents.forEach(r => {
    const card = document.createElement("div");
    card.className = `entity-card ${r.status}`;

    const matchText = r.match !== -1 ? currentDataset.hospitals[r.match].name : "UNMATCHED";
    const matchClass = r.match !== -1 ? "match-target matched" : "match-target";

    let pillsHtml = "";
    r.prefs.forEach(hId => {
      const hName = currentDataset.hospitals[hId].name;
      let pClass = "pref-pill";
      if (hId === r.match) pClass += " current";
      else if (hId === r.activeHospital) pClass += " active";
      pillsHtml += `<span class="${pClass}">${hName}</span>`;
    });

    card.innerHTML = `
      <div class="card-title-row">
        <span class="entity-name">${r.name}</span>
        <span class="${matchClass}">${matchText}</span>
      </div>
      <div class="pref-pills">${pillsHtml}</div>
    `;
    resContainer.appendChild(card);
  });

  // Render Hospitals
  const hospContainer = document.getElementById("hospitals-container");
  hospContainer.innerHTML = "";
  state.hospitals.forEach(h => {
    const card = document.createElement("div");
    const isFull = h.assigned.length >= h.capacity;
    card.className = `entity-card ${isFull ? 'matched' : ''}`;

    let slotsHtml = "";
    for (let c = 0; c < h.capacity; c++) {
      if (c < h.assigned.length) {
        const rName = currentDataset.residents[h.assigned[c]].name;
        slotsHtml += `<div class="roster-slot filled">${rName}</div>`;
      } else {
        slotsHtml += `<div class="roster-slot empty">vacant</div>`;
      }
    }
    if (h.capacity === 0) {
      slotsHtml = `<div class="roster-slot" style="color:var(--accent-red)">0 capacity</div>`;
    }

    card.innerHTML = `
      <div class="card-title-row">
        <span class="entity-name">${h.name}</span>
        <span class="match-target ${isFull ? 'matched' : ''}">${h.assigned.length} / ${h.capacity} seats</span>
      </div>
      <div class="roster-slots">${slotsHtml}</div>
    `;
    hospContainer.appendChild(card);
  });

  // Render Logs
  const logFeed = document.getElementById("log-feed");
  logFeed.innerHTML = "";
  state.logEntries.forEach(entry => {
    const div = document.createElement("div");
    div.className = `log-entry ${entry.type}`;
    div.innerText = entry.text;
    logFeed.appendChild(div);
  });
  logFeed.scrollTop = logFeed.scrollHeight;
}

function togglePlay() {
  isPlaying = !isPlaying;
  const playBtn = document.getElementById("btn-play");

  if (isPlaying) {
    playBtn.innerHTML = "&#x23F8; Pause";
    playBtn.classList.add("playing");

    if (currentStepIndex >= stateHistory.length - 1) {
      currentStepIndex = 0;
      renderState(0);
    }

    const intervalMs = Math.max(200, 1000 / playbackSpeed);
    playTimer = setInterval(() => {
      if (currentStepIndex < stateHistory.length - 1) {
        renderState(currentStepIndex + 1);
      } else {
        togglePlay();
      }
    }, intervalMs);
  } else {
    playBtn.innerHTML = "&#x25B6; Play";
    playBtn.classList.remove("playing");
    if (playTimer) clearInterval(playTimer);
  }
}

// Event Listeners
document.getElementById("btn-play").addEventListener("click", togglePlay);

document.getElementById("btn-next").addEventListener("click", () => {
  if (isPlaying) togglePlay();
  if (currentStepIndex < stateHistory.length - 1) {
    renderState(currentStepIndex + 1);
  }
});

document.getElementById("btn-prev").addEventListener("click", () => {
  if (isPlaying) togglePlay();
  if (currentStepIndex > 0) {
    renderState(currentStepIndex - 1);
  }
});

document.getElementById("btn-reset").addEventListener("click", () => {
  if (isPlaying) togglePlay();
  renderState(0);
});

document.getElementById("speed-slider").addEventListener("input", (e) => {
  playbackSpeed = parseFloat(e.target.value);
  document.getElementById("speed-val").innerText = `${playbackSpeed.toFixed(1)}x`;
  if (isPlaying) {
    togglePlay();
    togglePlay();
  }
});

document.getElementById("instance-select").addEventListener("change", (e) => {
  const key = e.target.value;
  if (PRESETS[key]) {
    initDataset(PRESETS[key]);
  }
});

// JSON File Upload
document.getElementById("json-file-input").addEventListener("change", (e) => {
  const file = e.target.files[0];
  if (!file) return;

  const reader = new FileReader();
  reader.onload = (event) => {
    try {
      const json = JSON.parse(event.target.result);
      if (json.residents && json.hospitals && json.steps) {
        initDataset(json);
      } else {
        alert("Invalid Gale-Shapley trace JSON format.");
      }
    } catch (err) {
      alert("Error parsing JSON file: " + err.message);
    }
  };
  reader.readAsText(file);
});

// Initialize on page load
window.addEventListener("DOMContentLoaded", () => {
  initDataset(PRESETS.spec);
});
