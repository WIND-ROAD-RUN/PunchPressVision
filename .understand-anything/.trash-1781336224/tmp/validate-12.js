const fs = require('fs');
const d = JSON.parse(fs.readFileSync('D:/Workplace/rep/PunchPressVision/.understand-anything/intermediate/batch-12.json','utf8'));

console.log('Valid JSON: YES');
console.log('Nodes:', d.nodes.length, 'Edges:', d.edges.length);

const ids = d.nodes.map(n => n.id);
const dupIds = ids.filter((id, i) => ids.indexOf(id) !== i);
console.log('Duplicate IDs:', dupIds.length > 0 ? dupIds : 'NONE');

const selfEdges = d.edges.filter(e => e.source === e.target);
console.log('Self-ref edges:', selfEdges.length > 0 ? selfEdges.length : 'NONE');

const idSet = new Set(ids);
const missingTargets = d.edges.filter(e => !idSet.has(e.target));
console.log('Missing targets:', missingTargets.length > 0 ? missingTargets.length : 'NONE');

const missingSources = d.edges.filter(e => !idSet.has(e.source));
console.log('Missing sources:', missingSources.length > 0 ? missingSources.length : 'NONE');

const emptyTags = d.nodes.filter(n => !Array.isArray(n.tags) || n.tags.length === 0);
console.log('Empty tags:', emptyTags.length > 0 ? emptyTags.map(n=>n.id) : 'NONE');

const funcClassNodes = d.nodes.filter(n => n.type === 'function' || n.type === 'class');
const missingLineRange = funcClassNodes.filter(n => !n.lineRange);
console.log('Missing lineRange:', missingLineRange.length > 0 ? missingLineRange.map(n=>n.id) : 'NONE');
