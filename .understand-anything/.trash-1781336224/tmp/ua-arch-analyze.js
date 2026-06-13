#!/usr/bin/env node
/**
 * Phase 1: Structural Analysis Script for Architecture Analyzer
 *
 * Reads phase4-input.json and computes structural patterns for layer identification.
 */

const fs = require('fs');
const path = require('path');

// Parse arguments
const inputPath = process.argv[2];
const outputPath = process.argv[3];

if (!inputPath || !outputPath) {
  console.error('Usage: node ua-arch-analyze.js <input.json> <output.json>');
  process.exit(1);
}

try {
  // ============================================================
  // Load input data
  // ============================================================
  const data = JSON.parse(fs.readFileSync(inputPath, 'utf-8'));
  const { fileNodes, importEdges, allEdges } = data;

  // ============================================================
  // A. Directory Grouping
  // ============================================================

  // Find common path prefix
  const filePaths = fileNodes.map(n => n.filePath);

  function findCommonPrefix(paths) {
    if (paths.length === 0) return '';
    const parts = paths.map(p => p.split('/'));
    const minLen = Math.min(...parts.map(p => p.length));
    let commonParts = [];
    for (let i = 0; i < minLen; i++) {
      const segment = parts[0][i];
      if (parts.every(p => p[i] === segment)) {
        commonParts.push(segment);
      } else {
        break;
      }
    }
    return commonParts.join('/');
  }

  const commonPrefix = findCommonPrefix(filePaths);

  // Group by first directory segment after common prefix
  const directoryGroups = {};

  fileNodes.forEach(node => {
    const fp = node.filePath;
    let remainder;

    if (commonPrefix && fp.startsWith(commonPrefix + '/')) {
      remainder = fp.substring(commonPrefix.length + 1);
    } else if (commonPrefix && fp === commonPrefix) {
      remainder = '';
    } else {
      remainder = fp;
    }

    let groupKey;
    if (remainder === '' || !remainder.includes('/')) {
      // Root-level file
      groupKey = '(root)';
    } else {
      // Use first 2 path segments for better grouping
      const parts = remainder.split('/');
      if (parts.length >= 2) {
        groupKey = parts.slice(0, 2).join('/');
      } else {
        groupKey = parts[0];
      }
    }

    if (!directoryGroups[groupKey]) {
      directoryGroups[groupKey] = [];
    }
    directoryGroups[groupKey].push(node.id);
  });

  // ============================================================
  // B. Node Type Grouping
  // ============================================================

  const nodeTypeGroups = {};
  fileNodes.forEach(node => {
    if (!nodeTypeGroups[node.type]) {
      nodeTypeGroups[node.type] = [];
    }
    nodeTypeGroups[node.type].push(node.id);
  });

  // ============================================================
  // C. Import Adjacency Matrix (fan-in / fan-out)
  // ============================================================

  const fileFanIn = {};
  const fileFanOut = {};

  importEdges.forEach(edge => {
    fileFanOut[edge.source] = (fileFanOut[edge.source] || 0) + 1;
    fileFanIn[edge.target] = (fileFanIn[edge.target] || 0) + 1;
  });

  // ============================================================
  // D. Cross-Category Dependency Analysis
  // ============================================================

  const crossCategoryEdges = {};

  allEdges.forEach(edge => {
    // Determine node types for source and target
    const srcNode = fileNodes.find(n => n.id === edge.source);
    const tgtNode = fileNodes.find(n => n.id === edge.target);

    if (srcNode && tgtNode) {
      // Only count edges between file-level nodes (not sub-file nodes)
      const srcType = srcNode.type;
      const tgtType = tgtNode.type;
      const key = `${srcType}->${tgtType}:${edge.type}`;
      crossCategoryEdges[key] = (crossCategoryEdges[key] || 0) + 1;
    }
  });

  const crossCategoryOutput = Object.entries(crossCategoryEdges).map(([key, count]) => {
    const [fromTo, edgeType] = key.split(':');
    const [fromType, toType] = fromTo.split('->');
    return { fromType, toType, edgeType, count };
  });

  // ============================================================
  // E. Inter-Group Import Frequency
  // ============================================================

  // Map file IDs to their directory groups
  const fileToGroup = {};
  Object.entries(directoryGroups).forEach(([group, ids]) => {
    ids.forEach(id => { fileToGroup[id] = group; });
  });

  const interGroupImports = {};
  importEdges.forEach(edge => {
    const fromGroup = fileToGroup[edge.source];
    const toGroup = fileToGroup[edge.target];
    if (fromGroup && toGroup && fromGroup !== toGroup) {
      const key = `${fromGroup}->${toGroup}`;
      interGroupImports[key] = (interGroupImports[key] || 0) + 1;
    }
  });

  const interGroupOutput = Object.entries(interGroupImports).map(([key, count]) => {
    const [from, to] = key.split('->');
    return { from, to, count };
  });

  // ============================================================
  // F. Intra-Group Import Density
  // ============================================================

  const intraGroupDensity = {};
  Object.entries(directoryGroups).forEach(([group, ids]) => {
    const idSet = new Set(ids);
    let internalEdges = 0;
    let totalEdges = 0;

    importEdges.forEach(edge => {
      const srcInGroup = idSet.has(edge.source);
      const tgtInGroup = idSet.has(edge.target);

      if (srcInGroup && tgtInGroup) {
        internalEdges++;
      }
      if (srcInGroup || tgtInGroup) {
        totalEdges++;
      }
    });

    intraGroupDensity[group] = {
      internalEdges,
      totalEdges,
      density: totalEdges > 0 ? internalEdges / totalEdges : 0
    };
  });

  // ============================================================
  // G. Directory Pattern Matching
  // ============================================================

  const patternMap = {
    'routes': 'api', 'api': 'api', 'controllers': 'api', 'endpoints': 'api', 'handlers': 'api',
    'services': 'service', 'core': 'service', 'lib': 'service', 'domain': 'service', 'logic': 'service',
    'models': 'data', 'db': 'data', 'data': 'data', 'persistence': 'data', 'repository': 'data', 'entities': 'data',
    'components': 'ui', 'views': 'ui', 'pages': 'ui', 'ui': 'ui', 'layouts': 'ui', 'screens': 'ui',
    'middleware': 'middleware', 'plugins': 'middleware', 'interceptors': 'middleware', 'guards': 'middleware',
    'utils': 'utility', 'helpers': 'utility', 'common': 'utility', 'shared': 'utility', 'tools': 'utility',
    'config': 'config', 'constants': 'config', 'env': 'config', 'settings': 'config',
    '__tests__': 'test', 'test': 'test', 'tests': 'test', 'spec': 'test', 'specs': 'test',
    'types': 'types', 'interfaces': 'types', 'schemas': 'types', 'contracts': 'types', 'dtos': 'types',
    'hooks': 'hooks', 'store': 'state', 'state': 'state', 'reducers': 'state', 'actions': 'state', 'slices': 'state',
    'assets': 'assets', 'static': 'assets', 'public': 'assets',
    'migrations': 'data', 'management': 'config', 'commands': 'config',
    'templatetags': 'utility', 'signals': 'service', 'serializers': 'api',
    'cmd': 'entry', 'internal': 'service', 'pkg': 'utility',
    'src/main/java': 'service', 'src/test/java': 'test',
    'dto': 'types', 'request': 'types', 'response': 'types',
    'entity': 'data', 'controller': 'api', 'routers': 'api',
    'composables': 'service', 'blueprints': 'api',
    'mailers': 'service', 'jobs': 'service', 'channels': 'service',
    'bin': 'entry', 'docs': 'documentation', 'documentation': 'documentation', 'wiki': 'documentation',
    'deploy': 'infrastructure', 'deployment': 'infrastructure', 'infra': 'infrastructure', 'infrastructure': 'infrastructure',
    '.github': 'ci-cd', '.gitlab': 'ci-cd', '.circleci': 'ci-cd',
    'k8s': 'infrastructure', 'kubernetes': 'infrastructure', 'helm': 'infrastructure', 'charts': 'infrastructure',
    'terraform': 'infrastructure', 'tf': 'infrastructure', 'docker': 'infrastructure',
    'sql': 'data', 'database': 'data', 'schema': 'data',
    'include': 'types', 'src': 'service',
  };

  const patternMatches = {};

  Object.keys(directoryGroups).forEach(group => {
    const lastSegment = group.split('/').pop().toLowerCase();
    const firstSegment = group.split('/')[0].toLowerCase();

    // Try exact match on last segment
    if (patternMap[lastSegment]) {
      patternMatches[group] = patternMap[lastSegment];
    } else if (patternMap[firstSegment]) {
      patternMatches[group] = patternMap[firstSegment];
    } else {
      // Check file-level patterns
      const ids = directoryGroups[group];
      const testFiles = ids.filter(id => {
        const name = id.split(':').pop().split('/').pop();
        return /\.(test|spec)\./.test(name) ||
               /^test_/.test(name) ||
               /_test\./.test(name) ||
               /Test\./.test(name) ||
               /_spec\./.test(name) ||
               /Tests\./.test(name);
      });

      if (testFiles.length > ids.length * 0.5) {
        patternMatches[group] = 'test';
      } else {
        patternMatches[group] = 'other';
      }
    }
  });

  // ============================================================
  // H. Deployment Topology Detection
  // ============================================================

  const deploymentTopology = {
    hasDockerfile: false,
    hasCompose: false,
    hasK8s: false,
    hasTerraform: false,
    hasCI: false,
    infraFiles: []
  };

  fileNodes.forEach(node => {
    const name = node.filePath.split('/').pop().toLowerCase();
    const fp = node.filePath.toLowerCase();

    if (name === 'dockerfile' || name.startsWith('dockerfile.')) {
      deploymentTopology.hasDockerfile = true;
      deploymentTopology.infraFiles.push(node.filePath);
    }
    if (name.startsWith('docker-compose')) {
      deploymentTopology.hasCompose = true;
      deploymentTopology.infraFiles.push(node.filePath);
    }
    if (fp.includes('.github/workflows/') || name === '.gitlab-ci.yml' || name === 'jenkinsfile') {
      deploymentTopology.hasCI = true;
      deploymentTopology.infraFiles.push(node.filePath);
    }
    if (fp.includes('/k8s/') || fp.includes('/kubernetes/') || fp.includes('/helm/')) {
      deploymentTopology.hasK8s = true;
      deploymentTopology.infraFiles.push(node.filePath);
    }
    if (name.endsWith('.tf') || name.endsWith('.tfvars')) {
      deploymentTopology.hasTerraform = true;
      deploymentTopology.infraFiles.push(node.filePath);
    }
  });

  // ============================================================
  // I. Data Pipeline Detection
  // ============================================================

  const dataPipeline = {
    schemaFiles: [],
    migrationFiles: [],
    dataModelFiles: [],
    apiHandlerFiles: []
  };

  fileNodes.forEach(node => {
    const fp = node.filePath;
    const name = fp.split('/').pop();

    if (/\.(sql|graphql|gql|proto|prisma)$/i.test(name) || fp.includes('/schema/')) {
      dataPipeline.schemaFiles.push(node.id);
    }
    if (fp.includes('/migrations/') || fp.includes('/migration/')) {
      dataPipeline.migrationFiles.push(node.id);
    }
    if (fp.includes('/models/') || fp.includes('/entities/') || fp.includes('/entity/')) {
      dataPipeline.dataModelFiles.push(node.id);
    }
    if (fp.includes('/routes/') || fp.includes('/api/') || fp.includes('/controllers/') || fp.includes('/handlers/')) {
      dataPipeline.apiHandlerFiles.push(node.id);
    }
  });

  // ============================================================
  // J. Documentation Coverage
  // ============================================================

  const groupsWithDocs = new Set();
  const undocumentedGroups = [];

  // Check for README.md in each group
  Object.entries(directoryGroups).forEach(([group, ids]) => {
    const hasDoc = ids.some(id => {
      const fpath = id.split(':').slice(2).join(':') || fileNodes.find(n => n.id === id)?.filePath || '';
      return fpath.toLowerCase().includes('readme') || fpath.toLowerCase().includes('.md');
    });
    if (hasDoc) {
      groupsWithDocs.add(group);
    }
  });

  // Check docs/ files referencing code groups
  fileNodes.forEach(node => {
    if (node.filePath.toLowerCase().startsWith('docs/') || node.filePath.endsWith('.md')) {
      // Mark the group of any file referenced in doc edges
      const docEdges = allEdges.filter(e =>
        e.source === node.id && e.type === 'documents'
      );
      docEdges.forEach(edge => {
        const tgtGroup = fileToGroup[edge.target];
        if (tgtGroup) groupsWithDocs.add(tgtGroup);
      });
    }
  });

  const totalGroups = Object.keys(directoryGroups).length;
  Object.keys(directoryGroups).forEach(g => {
    if (!groupsWithDocs.has(g)) undocumentedGroups.push(g);
  });

  const docCoverage = {
    groupsWithDocs: groupsWithDocs.size,
    totalGroups,
    coverageRatio: totalGroups > 0 ? groupsWithDocs.size / totalGroups : 0,
    undocumentedGroups
  };

  // ============================================================
  // K. Dependency Direction
  // ============================================================

  const dependencyDirection = [];
  const processedPairs = new Set();

  importEdges.forEach(edge => {
    const fromGroup = fileToGroup[edge.source];
    const toGroup = fileToGroup[edge.target];

    if (fromGroup && toGroup && fromGroup !== toGroup) {
      const pairKey = [fromGroup, toGroup].sort().join('|||');
      if (processedPairs.has(pairKey)) return;
      processedPairs.add(pairKey);

      // Count edges in each direction
      let aToB = 0, bToA = 0;
      importEdges.forEach(e => {
        const fg = fileToGroup[e.source];
        const tg = fileToGroup[e.target];
        if (fg === fromGroup && tg === toGroup) aToB++;
        if (fg === toGroup && tg === fromGroup) bToA++;
      });

      if (aToB > bToA) {
        dependencyDirection.push({ dependent: fromGroup, dependsOn: toGroup });
      } else if (bToA > aToB) {
        dependencyDirection.push({ dependent: toGroup, dependsOn: fromGroup });
      }
      // If equal, no dominant direction
    }
  });

  // ============================================================
  // File Stats
  // ============================================================

  const filesPerGroup = {};
  Object.entries(directoryGroups).forEach(([group, ids]) => {
    filesPerGroup[group] = ids.length;
  });

  const nodeTypeCounts = {};
  fileNodes.forEach(node => {
    nodeTypeCounts[node.type] = (nodeTypeCounts[node.type] || 0) + 1;
  });

  // ============================================================
  // Build and write output
  // ============================================================

  const output = {
    scriptCompleted: true,
    directoryGroups,
    nodeTypeGroups,
    crossCategoryEdges: crossCategoryOutput,
    interGroupImports: interGroupOutput,
    intraGroupDensity,
    patternMatches,
    deploymentTopology,
    dataPipeline,
    docCoverage,
    dependencyDirection,
    fileStats: {
      totalFileNodes: fileNodes.length,
      filesPerGroup,
      nodeTypeCounts
    },
    fileFanIn,
    fileFanOut
  };

  fs.writeFileSync(outputPath, JSON.stringify(output, null, 2), 'utf-8');
  console.log(`Analysis complete. ${fileNodes.length} file nodes, ${importEdges.length} import edges, ${allEdges.length} total edges.`);
  console.log(`${Object.keys(directoryGroups).length} directory groups identified.`);
  process.exit(0);

} catch (err) {
  console.error('Fatal error:', err.message);
  console.error(err.stack);
  process.exit(1);
}
