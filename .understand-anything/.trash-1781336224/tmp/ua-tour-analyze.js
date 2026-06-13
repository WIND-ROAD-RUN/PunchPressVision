// Phase 1: Graph Topology Analysis Script
// Reads the input JSON (phase5-input.json), computes structural properties, writes results.

const fs = require('fs');

// ------ Parse arguments ------
const inputPath = process.argv[2];
const outputPath = process.argv[3];

if (!inputPath || !outputPath) {
  console.error('Usage: node ua-tour-analyze.js <input.json> <output.json>');
  process.exit(1);
}

// ------ Read input ------
let data;
try {
  data = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
} catch (e) {
  console.error('Failed to read or parse input file:', e.message);
  process.exit(1);
}

// Input uses "fileNodes" key (not "nodes")
const rawNodes = data.fileNodes || [];
const edges = data.edges || [];
const layers = data.layers || [];

// ------ Helper: resolve a node ID (which could be class:... or function:...) to its file-level ID ------
function resolveToFile(nodeId) {
  if (!nodeId) return null;
  // If it starts with file:, config:, document:, it's already a file-level node
  if (nodeId.startsWith('file:') || nodeId.startsWith('config:') || nodeId.startsWith('document:')) {
    return nodeId;
  }
  // For class:path:ClassName, function:path:FuncName, extract the file path part
  // Format: class:relative/path/to/file.hpp:ClassName
  // or:     function:relative/path/to/file.cpp:funcName
  const parts = nodeId.split(':');
  if (parts.length >= 3) {
    // The file path is parts[1] (after the type prefix, before the symbol name)
    const filePath = parts.slice(1, -1).join(':');
    // Try to find in node set
    return 'file:' + filePath;
  }
  return null;
}

// Build node lookup
const nodeMap = {};   // file-level id -> node object
const nodeIdSet = new Set();
for (const n of rawNodes) {
  nodeMap[n.id] = n;
  nodeIdSet.add(n.id);
}

// Also build a reverse map: file path -> any node id that starts with that path
// This helps resolve class: nodes back to file: nodes
function resolveNodeId(rawId) {
  if (nodeIdSet.has(rawId)) return rawId;
  // Try file: prefix
  const fileId = 'file:' + (rawId.startsWith('class:') || rawId.startsWith('function:')
    ? rawId.split(':').slice(1, -1).join(':')
    : rawId);
  if (nodeIdSet.has(fileId)) return fileId;
  return rawId; // return as-is, may be resolved later
}

// ------ Aggregate edges to file level ------
// For each file-level node, track fan-in and fan-out
const fanIn = {};   // nodeId -> Set of source nodeIds
const fanOut = {};  // nodeId -> Set of target nodeIds

// Initialize
for (const n of rawNodes) {
  fanIn[n.id] = new Set();
  fanOut[n.id] = new Set();
}

// Edge types that represent actual code dependencies (not hierarchical like contains/exports)
const dependencyEdgeTypes = new Set([
  'imports', 'calls', 'depends_on', 'configures', 'inherits', 'related', 'defines_schema'
]);

// First pass: build a reverse map from class/function nodes to their containing file nodes
const symbolToFile = {}; // class:path:Name -> file:path
for (const edge of edges) {
  if (edge.type === 'contains' || edge.type === 'exports') {
    const srcFile = resolveNodeId(edge.source);
    const tgtSymbol = edge.target;
    if (srcFile && nodeIdSet.has(srcFile)) {
      symbolToFile[tgtSymbol] = srcFile;
    }
  }
}

// Process each edge — dependency edges for file-level topology
// Also resolve through contains: if file A contains symbol S, and symbol S calls symbol T in file B,
// then we can infer file A -> file B
for (const edge of edges) {
  const srcRaw = edge.source;
  const tgtRaw = edge.target;

  // For dependency edges, resolve to file level
  let srcFile = null;
  let tgtFile = null;

  if (dependencyEdgeTypes.has(edge.type)) {
    srcFile = resolveNodeId(srcRaw);
    tgtFile = resolveNodeId(tgtRaw);
  }

  // Try resolving through contains: if source is a symbol, find its containing file
  if (!srcFile || !nodeIdSet.has(srcFile)) {
    srcFile = symbolToFile[srcRaw] || null;
  }
  if (!tgtFile || !nodeIdSet.has(tgtFile)) {
    tgtFile = symbolToFile[tgtRaw] || null;
  }

  // Skip if we can't resolve
  if (!srcFile || !tgtFile || !nodeIdSet.has(srcFile) || !nodeIdSet.has(tgtFile)) continue;
  // Skip self-loops
  if (srcFile === tgtFile) continue;

  fanOut[srcFile].add(tgtFile);
  fanIn[tgtFile].add(srcFile);
}

// Convert Sets to counts
const totalNodes = rawNodes.length;
const totalEdges = edges.length;

// ------ A. Fan-In Ranking ------
const fanInRanking = rawNodes
  .map(n => ({ id: n.id, fanIn: fanIn[n.id].size, name: n.name }))
  .sort((a, b) => b.fanIn - a.fanIn)
  .slice(0, 20);

// ------ B. Fan-Out Ranking ------
const fanOutRanking = rawNodes
  .map(n => ({ id: n.id, fanOut: fanOut[n.id].size, name: n.name }))
  .sort((a, b) => b.fanOut - a.fanOut)
  .slice(0, 20);

// ------ C. Entry Point Candidates ------
const entryPoints = rawNodes.map(n => {
  let score = 0;
  const name = n.name.toLowerCase();
  const filePath = n.filePath || '';

  // Code file scoring
  if (n.type === 'file') {
    const entryNames = ['index.ts', 'index.js', 'main.ts', 'main.js', 'app.ts', 'app.js',
      'server.ts', 'server.js', 'mod.rs', 'main.go', 'main.py', 'main.rs',
      'manage.py', 'app.py', 'wsgi.py', 'asgi.py', 'run.py', '__main__.py',
      'application.java', 'main.java', 'program.cs', 'config.ru', 'index.php',
      'app.swift', 'application.kt', 'main.cpp', 'main.c'];
    if (entryNames.includes(name)) score += 3;

    // File is at project root or one level deep
    // Count path segments (excluding the file itself)
    const dirParts = filePath.replace(/\\/g, '/').split('/');
    // For paths like "PunchPressVision/UI/src/main.cpp", the root folder is PunchPressVision
    const relativeDepth = dirParts.length - 1; // number of directories deep
    // "At root or one level" = file in root dir or 1 dir deep from project root
    // But since paths start with PunchPressVision/, we check relative to that
    // Simpler: check if file has <= 2 path segments after PunchPressVision/
    const segments = dirParts;
    if (segments.length <= 2) score += 1; // e.g., "PunchPressVision/main.cpp" or "PunchPressVision/src/main.cpp"

    // High fan-out (top 10%)
    const fanOutCount = fanOut[n.id].size;
    const fanOutThreshold = Math.max(...rawNodes.map(x => fanOut[x.id].size)) * 0.9;
    if (fanOutCount >= fanOutThreshold && fanOutCount > 0) score += 1;

    // Low fan-in (bottom 25%)
    const fanInCount = fanIn[n.id].size;
    const allFanIns = rawNodes.map(x => fanIn[x.id].size).sort((a, b) => a - b);
    const p25Idx = Math.floor(allFanIns.length * 0.25);
    const p25Threshold = allFanIns[p25Idx] || 0;
    if (fanInCount <= p25Threshold) score += 1;
  }

  // Documentation scoring
  if (n.type === 'document') {
    if (name === 'readme.md' && (filePath === 'README.md' || filePath === 'readme.md' ||
        filePath === 'CLAUDE.md')) {
      score += 5;
    } else if (name.endsWith('.md')) {
      // Check if at project root
      const segments = filePath.replace(/\\/g, '/').split('/');
      if (segments.length <= 1) score += 2;
    }
  }

  return {
    id: n.id,
    score,
    name: n.name,
    summary: n.summary || ''
  };
});

entryPoints.sort((a, b) => b.score - a.score);
const entryPointCandidates = entryPoints.slice(0, 5);

// ------ D. BFS Traversal from Top Code Entry Point ------
// Find the top code entry point (skip documents)
let bfsStartNode = null;
for (const ep of entryPointCandidates) {
  if (ep.id.startsWith('file:') && nodeIdSet.has(ep.id)) {
    bfsStartNode = ep.id;
    break;
  }
}
// Fallback: pick the file with highest fan-out
if (!bfsStartNode) {
  const fanOutArr = rawNodes
    .filter(n => n.type === 'file' && fanOut[n.id].size > 0)
    .sort((a, b) => fanOut[b.id].size - fanOut[a.id].size);
  if (fanOutArr.length > 0) bfsStartNode = fanOutArr[0].id;
}

// Also add a secondary BFS from the highest-fan-out CODE file for depth discovery
// (when the entry point has no outgoing edges, we still want to discover the dependency graph)
let secondaryBfs = null;
const fanOutCodeArr = rawNodes
  .filter(n => n.type === 'file' && !n.id.includes('CMakeLists') && !n.id.includes('.cmake') && fanOut[n.id].size > 0)
  .sort((a, b) => fanOut[b.id].size - fanOut[a.id].size);
if (fanOutCodeArr.length > 0) {
  const secStart = fanOutCodeArr[0].id;
  if (secStart !== bfsStartNode) {
    const visited = new Set();
    const queue = [{ id: secStart, depth: 0 }];
    const order = [];
    const depthMap = {};
    const byDepth = {};
    visited.add(secStart);
    while (queue.length > 0) {
      const { id, depth } = queue.shift();
      order.push(id);
      depthMap[id] = depth;
      if (!byDepth[depth]) byDepth[depth] = [];
      byDepth[depth].push(id);
      const neighbors = fanOut[id] || new Set();
      for (const neighbor of neighbors) {
        if (!visited.has(neighbor)) {
          visited.add(neighbor);
          queue.push({ id: neighbor, depth: depth + 1 });
        }
      }
    }
    secondaryBfs = { startNode: secStart, order, depthMap, byDepth };
  }
}

const bfsOrder = [];
const depthMap = {};
const byDepth = {};

if (bfsStartNode && nodeIdSet.has(bfsStartNode)) {
  const visited = new Set();
  const queue = [{ id: bfsStartNode, depth: 0 }];
  visited.add(bfsStartNode);

  while (queue.length > 0) {
    const { id, depth } = queue.shift();
    bfsOrder.push(id);
    depthMap[id] = depth;
    if (!byDepth[depth]) byDepth[depth] = [];
    byDepth[depth].push(id);

    // Get neighbors via imports + calls edges
    const neighbors = fanOut[id] || new Set();
    for (const neighbor of neighbors) {
      // Only follow imports and calls (we already aggregated to file level)
      if (!visited.has(neighbor)) {
        visited.add(neighbor);
        queue.push({ id: neighbor, depth: depth + 1 });
      }
    }
  }
}

// Merge secondary BFS into primary if primary yielded very few nodes
if (secondaryBfs && bfsOrder.length <= 3 && secondaryBfs.order.length > bfsOrder.length) {
  // Use secondary BFS as the main traversal for discovering dependencies
  const mergedOrder = [...new Set([...bfsOrder, ...secondaryBfs.order])];
  const mergedDepthMap = { ...secondaryBfs.depthMap };
  for (const id of bfsOrder) {
    if (!(id in mergedDepthMap)) mergedDepthMap[id] = 0;
  }
  const mergedByDepth = { ...secondaryBfs.byDepth };
  // Add entry point nodes at depth 0
  for (const id of bfsOrder) {
    if (!mergedByDepth[0]) mergedByDepth[0] = [];
    if (!mergedByDepth[0].includes(id)) mergedByDepth[0].push(id);
  }
  // Re-index depths
  const allDepths = Object.keys(mergedByDepth).map(Number).sort((a,b)=>a-b);
  const reindexedByDepth = {};
  allDepths.forEach(d => { reindexedByDepth[d] = mergedByDepth[d]; });

  Object.assign(bfsOrder, mergedOrder);
  Object.assign(depthMap, mergedDepthMap);
  Object.assign(byDepth, reindexedByDepth);
  bfsStartNode = secondaryBfs.startNode;
}

const bfsTraversal = {
  startNode: bfsStartNode,
  order: bfsOrder,
  depthMap,
  byDepth
};

// ------ E. Non-Code File Inventory ------
const nonCodeFiles = {
  documentation: [],
  infrastructure: [],
  data: [],
  config: []
};

// Document types
for (const n of rawNodes) {
  if (n.type === 'document') {
    nonCodeFiles.documentation.push({
      id: n.id,
      name: n.name,
      summary: n.summary || ''
    });
  }
}

// Config types (config: prefix)
for (const n of rawNodes) {
  if (n.type === 'config') {
    nonCodeFiles.config.push({
      id: n.id,
      name: n.name,
      summary: n.summary || ''
    });
  }
}

// Infrastructure types: service, pipeline, resource
for (const n of rawNodes) {
  if (n.type === 'service' || n.type === 'pipeline' || n.type === 'resource') {
    nonCodeFiles.infrastructure.push({
      id: n.id,
      name: n.name,
      summary: n.summary || ''
    });
  }
}

// Data/Schema types: table, schema, endpoint
for (const n of rawNodes) {
  if (n.type === 'table' || n.type === 'schema' || n.type === 'endpoint') {
    nonCodeFiles.data.push({
      id: n.id,
      name: n.name,
      summary: n.summary || ''
    });
  }
}

// ------ F. Tightly Coupled Clusters ------
// Find bidirectional relationships at file level
const bidirectionalPairs = [];
const processedPairs = new Set();

for (const n of rawNodes) {
  const outs = fanOut[n.id];
  for (const target of outs) {
    const pairKey = [n.id, target].sort().join('|||');
    if (processedPairs.has(pairKey)) continue;
    processedPairs.add(pairKey);

    // Check if target also points to n
    if (fanOut[target] && fanOut[target].has(n.id)) {
      bidirectionalPairs.push([n.id, target]);
    }
  }
}

// Expand clusters: add nodes that connect to 2+ existing cluster members
const clusters = [];
const assignedNodes = new Set();

for (const pair of bidirectionalPairs) {
  const [a, b] = pair;
  if (assignedNodes.has(a) && assignedNodes.has(b)) continue;

  // Start a cluster
  let cluster = [a, b];
  let clusterSet = new Set(cluster);

  // Try to expand
  let changed = true;
  while (changed && cluster.length < 5) {
    changed = false;
    for (const n of rawNodes) {
      if (clusterSet.has(n.id)) continue;
      // Count connections to cluster members
      let connections = 0;
      for (const member of cluster) {
        if (fanOut[n.id] && fanOut[n.id].has(member)) connections++;
        if (fanOut[member] && fanOut[member].has(n.id)) connections++;
      }
      if (connections >= 2) {
        cluster.push(n.id);
        clusterSet.add(n.id);
        changed = true;
        if (cluster.length >= 5) break;
      }
    }
  }

  // Only keep clusters with 2-5 nodes
  if (cluster.length >= 2 && cluster.length <= 5) {
    // Count edges within cluster
    let edgeCount = 0;
    for (const src of cluster) {
      for (const tgt of cluster) {
        if (src !== tgt && fanOut[src] && fanOut[src].has(tgt)) edgeCount++;
      }
    }
    clusters.push({ nodes: cluster, edgeCount });
    for (const n of cluster) assignedNodes.add(n);
  }
}

// Sort clusters by edge count, keep top 10
clusters.sort((a, b) => b.edgeCount - a.edgeCount);
const topClusters = clusters.slice(0, 10);

// ------ G. Layer List ------
const layerOutput = {
  count: layers.length,
  list: layers.map(l => ({ id: l.id, name: l.name, description: l.description || '' }))
};

// ------ H. Node Summary Index ------
const nodeSummaryIndex = {};
for (const n of rawNodes) {
  nodeSummaryIndex[n.id] = {
    name: n.name,
    type: n.type,
    summary: n.summary || ''
  };
}

// ------ Assemble output ------
const result = {
  scriptCompleted: true,
  entryPointCandidates,
  fanInRanking,
  fanOutRanking,
  bfsTraversal,
  nonCodeFiles,
  clusters: topClusters,
  layers: layerOutput,
  nodeSummaryIndex,
  totalNodes,
  totalEdges
};

fs.writeFileSync(outputPath, JSON.stringify(result, null, 2), 'utf8');
console.log('Script completed successfully.');
console.log(`Total nodes: ${totalNodes}`);
console.log(`Total edges: ${totalEdges}`);
console.log(`Entry points: ${entryPointCandidates.map(e => `${e.name}(${e.score})`).join(', ')}`);
console.log(`Top fan-in: ${fanInRanking[0]?.name}(${fanInRanking[0]?.fanIn})`);
console.log(`BFS start: ${bfsStartNode}`);
console.log(`BFS order length: ${bfsOrder.length}`);
console.log(`Clusters: ${topClusters.length}`);
