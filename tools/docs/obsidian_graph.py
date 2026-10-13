#!/usr/bin/env python3
"""Obsidian graph links for the docs of RunicEngine, RunicPlatform and RunicGame.

For every docs/**/*.md note it adds, without changing the existing text:
  * YAML frontmatter with tags (repo, type, group, source, slice, ...) and aliases;
  * a footnote marker [^label] after the first mention of a connected note in each H2 section;
  * a generated footer ("## Connections" plus the footnote definitions), each entry a relative
    [[wikilink|Name]] so Obsidian draws the edge.
It is idempotent: the generated parts are stripped and regenerated on every run, so rerun it after any
docs change instead of editing them by hand. Mirrored docs/system copies stay byte-identical, and the
public repositories link only to RunicGame's work-package list, never to Game-specific notes.

Vault: one Obsidian vault whose root holds the folders RunicEngine, RunicPlatform and RunicGame.
Usage, from the folder that holds the three repositories:
    python3 RunicEngine/tools/docs/obsidian_graph.py RunicEngine RunicPlatform RunicGame [--write]
Without --write it only reports how many notes would change.
"""
import os, re, sys, posixpath

ENGINE_DIR, PLATFORM_DIR, GAME_DIR = sys.argv[1:4]
WRITE = '--write' in sys.argv
REPOS = {'engine': (ENGINE_DIR, 'RunicEngine'), 'platform': (PLATFORM_DIR, 'RunicPlatform'),
         'game': (GAME_DIR, 'RunicGame')}
PUBLIC = {'engine', 'platform'}

START = '<!-- obsidian-graph:start -->'
FM_MARK = '# obsidian-graph: generated tags and aliases'
LABEL_RE = r'(?:engine|platform|game|service|doc|oq|arch|vision)(?:-[a-z0-9-]+)?'

# ---------------------------------------------------------------- notes
notes = {}   # key -> dict(repo, rel (vault path w/o .md), name, kind)
def vault(repo, docrel):
    return posixpath.join(REPOS[repo][1], 'docs', docrel[:-3])

files = []   # (repo, docrel)
for repo, (root, _) in REPOS.items():
    for dp, dn, fn in os.walk(os.path.join(root, 'docs')):
        for f in sorted(fn):
            if f.endswith('.md'):
                files.append((repo, os.path.relpath(os.path.join(dp, f), os.path.join(root, 'docs')).replace(os.sep, '/')))

def strip_generated(text):
    if text.startswith('---\n') and FM_MARK in text.split('\n---\n', 1)[0]:
        text = text.split('\n---\n', 1)[1].lstrip('\n')
    if START in text:
        text = text[:text.index(START)].rstrip() + '\n'
    text = re.sub(r'\[\^' + LABEL_RE + r'\]', '', text)
    return text

def read(repo, docrel):
    return strip_generated(open(os.path.join(REPOS[repo][0], 'docs', docrel), encoding='utf-8').read())

# ---------------------------------------------------------------- catalogs
ENGINE_GROUPS = {'Foundation': 'foundation', 'Simulation core': 'simulation-core', 'GameplayAPI': 'gameplay-api',
                 'World systems': 'world-systems', 'Networking': 'networking', 'Presentation': 'presentation',
                 'Tools': 'tools'}
GROUP_LABEL = {v: k for k, v in ENGINE_GROUPS.items()}
engine = {}   # name -> dict(group, headless, allows[list or None], slice)
sys03 = read('engine', 'system/03-dependency-rules.md')
for line in sys03.splitlines():
    c = [x.strip() for x in line.strip().strip('|').split('|')]
    if len(c) == 6 and c[1].startswith('Runic::'):
        name = c[1].split('::')[1]
        deps = re.sub(r'\([^)]*\)', '', c[3]).split(';')[0].strip()
        if deps in ('—', ''):
            allows = []
        elif deps.startswith('Most modules'):
            allows = None
        else:
            allows = [d.strip() for d in deps.split(',') if d.strip()]
        h = c[4].lower()
        h = 'yes' if h.startswith('yes') else h.replace(' ', '-')
        engine[name] = dict(group=ENGINE_GROUPS[c[0]], headless=h, allows=allows, slice=None)
for line in read('engine', 'modules/README.md').splitlines():
    m = re.match(r'\| [^|]+ \| \[(\w+)\]\(\w+\.md\) \|.*\| (\d+[a-z]?) \| [^|]+ \|$', line)
    if m and m.group(1) in engine:
        engine[m.group(1)]['slice'] = m.group(2)

PLATFORM_LANG = {'Transport': 'cpp', 'Contracts': 'protobuf', 'ServerBindings': 'cpp', 'GoSDK': 'go',
                 'CoreServices': 'go', 'BuildingBlocks': 'go', 'Infrastructure': 'config'}
LANG_LABEL = {'cpp': 'C++', 'go': 'Go', 'protobuf': 'Protobuf', 'config': 'configuration'}
PLATFORM_DEPS = {'Transport': ['E:Network', 'P:Contracts'], 'Contracts': [],
                 'ServerBindings': ['E:Core', 'E:Application', 'E:Simulation', 'P:Contracts'],
                 'GoSDK': ['P:Contracts'], 'CoreServices': ['P:GoSDK', 'P:Contracts'],
                 'BuildingBlocks': ['P:GoSDK', 'P:Contracts'], 'Infrastructure': []}
PLATFORM_ALIASES = {'Transport': ['RunicPlatform::Transport', 'Platform Transport'],
                    'Contracts': ['RunicPlatform::Contracts', 'Platform Contracts'],
                    'ServerBindings': ['RunicPlatform::ServerBindings', 'Server bindings'],
                    'GoSDK': ['Go SDK'], 'CoreServices': ['Core services', 'World Coordinator'],
                    'BuildingBlocks': ['Building blocks', 'Item ledger', 'Currency wallets'],
                    'Infrastructure': ['Platform Infrastructure']}
platform = {}
for line in read('platform', 'modules/README.md').splitlines():
    m = re.match(r'\| \[(\w+)\]\(\w+\.md\) \|.*\| (\d+[a-z]?) \| [^|]+ \|$', line)
    if m and m.group(1) in PLATFORM_LANG:
        platform[m.group(1)] = dict(slice=m.group(2))

game_areas = {}
for line in read('game', 'modules/README.md').splitlines():
    m = re.match(r'\| \[(\w+)\]\(\w+\.md\) \| ([^|]+) \|.*\| ([^|]+) \| [^|]+ \|$', line)
    if m:
        t = m.group(2).strip().split(';')[0].split(',')[0].split(' ')[0].lower()
        game_areas[m.group(1)] = dict(target=t, slice=m.group(3).strip())
services = {}
for line in read('game', 'services/README.md').splitlines():
    m = re.match(r'\| \[([\w ]+)\]\((\w+)Service\.md\) \|.*\| ([^|]+) \| [^|]+ \|$', line)
    if m:
        services[m.group(2)] = dict(label=m.group(1), slice=m.group(3).strip())

for n in engine:
    notes['E:' + n] = dict(repo='engine', rel=vault('engine', f'modules/{n}.md'), name=n, kind='engine')
for n in platform:
    notes['P:' + n] = dict(repo='platform', rel=vault('platform', f'modules/{n}.md'), name=n, kind='platform')
for n in game_areas:
    notes['G:' + n] = dict(repo='game', rel=vault('game', f'modules/{n}.md'), name=n, kind='game')
for n, s in services.items():
    notes['S:' + n] = dict(repo='game', rel=vault('game', f'services/{n}Service.md'), name=f'{s["label"]} service', kind='service')

DOC_TITLES = {'README.md': 'docs README', 'architecture.md': 'architecture', 'current-state.md': 'current state',
              'build/BUILD-1.md': 'BUILD-1 contract', 'modules/README.md': 'module catalog',
              'services/README.md': 'service catalog', 'game-packages.md': 'work packages'}
REPO_TITLE = {'engine': 'Engine', 'platform': 'Platform', 'game': 'Game'}
def doc_key(repo, docrel):
    return f'D:{repo}:{docrel}'
for repo, docrel in files:
    if docrel.startswith('modules/') and docrel != 'modules/README.md':
        continue
    if docrel.startswith('services/') and docrel != 'services/README.md':
        continue
    if docrel.startswith('system/'):
        base = docrel[7:-3]
        title = {'README': 'System README', 'glossary': 'Glossary'}.get(base, base)
        if re.match(r'\d\d-', base):
            title = base[:2] + ' — ' + base[3:].replace('-', ' ').capitalize()
    else:
        title = f'{REPO_TITLE[repo]} {DOC_TITLES.get(docrel, docrel[:-3])}'
    notes[doc_key(repo, docrel)] = dict(repo=repo, rel=vault(repo, docrel), name=title, kind='doc', docrel=docrel)

# ---------------------------------------------------------------- packages
PREFIX = {'CORE': 'Core', 'APP': 'Application', 'EVT': 'Events', 'CNT': 'Content', 'ASSET': 'Assets',
          'ECS': 'ECS', 'WORLD': 'World', 'PHYS': 'Physics', 'SIM': 'Simulation', 'GPL': 'Gameplay',
          'CMB': 'Combat', 'FLOW': 'GameplayFlow', 'QST': 'Quest', 'PRC': 'Perception', 'NAV': 'Navigation',
          'AI': 'AI', 'ENV': 'Environment', 'WATER': 'Water', 'NET': 'Network', 'DSIM': 'DistributedSimulation',
          'CLI': 'Client', 'RND': 'Renderer', 'ANIM': 'Animation', 'UI': 'UI', 'AUD': 'Audio', 'EDT': 'Editor',
          'COOK': 'Cooker'}
PPREFIX = {'PCT': 'Contracts', 'PTR': 'Transport', 'PSDK': 'GoSDK', 'PBND': 'ServerBindings',
           'PCS': 'CoreServices', 'PBB': 'BuildingBlocks', 'PINF': 'Infrastructure'}
GPREFIX = ('GAME', 'GSH', 'GCNT', 'GCL', 'GSVC', 'GSRV', 'GNAV', 'GWTH')
PKG_RE = r'\b(?:BUILD|' + '|'.join(list(PREFIX) + list(PPREFIX) + list(GPREFIX)) + r')-\d+\b'
game_home = {}
pkg_needs = {}
for repo, docrel in [('engine', 'system/12-work-packages.md'), ('game', 'game-packages.md')]:
    for line in read(repo, docrel).splitlines():
        c = [x.strip() for x in line.strip().strip('|').split('|')]
        if len(c) >= 5 and re.fullmatch(PKG_RE, c[0]):
            pkg_needs[c[0]] = re.findall(PKG_RE, c[3])
            if len(c) >= 6:
                m = re.search(r'\]\((modules|services)/(\w+)\.md\)', c[5])
                if m:
                    game_home[c[0]] = ('G:' + m.group(2)) if m.group(1) == 'modules' else ('S:' + m.group(2)[:-7])

def pkg_owner(pid, note_repo, public):
    pre = pid.split('-')[0]
    if pre in PREFIX:
        return 'E:' + PREFIX[pre]
    if pre in PPREFIX:
        return 'P:' + PPREFIX[pre]
    if pre == 'BUILD':
        return f'D:{note_repo}:build/BUILD-1.md' if note_repo != 'system' else 'SYS:build/BUILD-1.md'
    if public:
        return 'GP'          # game work packages note, never a game-specific note
    return game_home.get(pid, doc_key('game', 'game-packages.md'))

def owned_packages(key):
    return sorted([p for p in pkg_needs if pkg_owner(p, 'game', False) == key],
                  key=lambda p: (p.split('-')[0], int(p.split('-')[1])))

# ---------------------------------------------------------------- OQ headings
oq_heading = {}
for line in read('engine', 'system/11-open-questions.md').splitlines():
    m = re.match(r'#{2,3} (OQ-(\d+) — .*)$', line)
    if m:
        oq_heading[m.group(2)] = m.group(1)

# ---------------------------------------------------------------- link helpers
def note_of(repo, docrel):
    return vault(repo, docrel) if repo != 'system' else None

def wikilink(src_rel, target_rel, alias, system_src=False):
    """Relative wikilink. With system_src (a mirrored system note linking a note of one specific repo),
    the link goes through the vault root (../../../<Repo>/docs/...) so the three copies stay identical."""
    src_dir = posixpath.dirname(src_rel)
    if system_src:
        return f'[[../../../{target_rel}|{alias}]]'
    rel = posixpath.relpath(target_rel, src_dir)
    if not rel.startswith('../'):
        rel = '../' + posixpath.basename(src_dir) + '/' + rel
    return f'[[{rel}|{alias}]]'

def resolve(key, src):
    """Vault path of a key, seen from note src (dict with repo, system flag)."""
    if key == 'GP':
        return vault('game', 'game-packages.md'), 'Game work packages'
    if key.startswith('SYS:'):
        # repo-relative doc seen from a mirrored system note: stays in the same repo
        docrel = key[4:]
        return posixpath.join(src['vault_root'], 'docs', docrel[:-3]), DOC_TITLES.get(docrel, docrel)
    n = notes[key]
    if n['kind'] == 'doc' and n['docrel'].startswith('system/'):
        if src['system']:      # a mirrored note links inside its own copy
            return posixpath.join(src['vault_root'], 'docs', n['docrel'][:-3]), n['name']
        # every other note links the canonical copy in RunicEngine, so one system cluster carries the graph
        return vault('engine', n['docrel']), n['name']
    return n['rel'], n['name']

def label_of(key):
    if key == 'GP':
        return 'doc-game-packages'
    if key.startswith('SYS:') or key.startswith('D:'):
        d = key.split(':')[-1][:-3]
        return 'doc-' + re.sub(r'[^a-z0-9]+', '-', d.lower()).strip('-')
    k, n = key.split(':', 1)
    return {'E': 'engine', 'P': 'platform', 'G': 'game', 'S': 'service'}[k] + '-' + n.lower()

def describe(key):
    if key == 'GP':
        return 'RunicGame work packages'
    if key.startswith('SYS:'):
        return 'this repository'
    n = notes[key]
    if n['kind'] == 'engine':
        e = engine[n['name']]
        return f'Engine module · {GROUP_LABEL[e["group"]]}'
    if n['kind'] == 'platform':
        return f'Platform module · {LANG_LABEL[PLATFORM_LANG[n["name"]]]}'
    if n['kind'] == 'game':
        return f'Game area · {game_areas[n["name"]]["target"]}'
    if n['kind'] == 'service':
        return 'Game service (Go)'
    return {'engine': 'RunicEngine', 'platform': 'RunicPlatform', 'game': 'RunicGame'}[n['repo']] + ' doc' \
        if not n['docrel'].startswith('system/') else 'System doc (mirrored)'

# ---------------------------------------------------------------- mention patterns
E_NAMES = sorted(engine, key=len, reverse=True)
E_DISTINCT = ['GameplayFlow', 'DistributedSimulation', 'Perception', 'Navigation', 'Renderer', 'Animation',
              'Cooker', 'Combat', 'Physics', 'ECS']
SVC_PHRASES = {s['label'].lower(): 'S:' + n for n, s in services.items()}
GAME_AREAS = sorted(game_areas, key=len, reverse=True)

def build_patterns(note_repo, public):
    pats = []   # (regex, resolver(match) -> key or None)
    pats.append((re.compile(r'Runic::(\w+)'), lambda m: 'E:Network' if m.group(1) == 'NetworkTransport' else
                 'E:Simulation' if m.group(1) == 'SimulationPort' else
                 ('E:' + m.group(1)) if m.group(1) in engine else None))
    pats.append((re.compile(r'RunicPlatform::(\w+)'), lambda m: ('P:' + m.group(1)) if m.group(1) in platform else None))
    pats.append((re.compile(PKG_RE), lambda m: pkg_owner(m.group(0), note_repo, public)))
    pats.append((re.compile(r'\bOQ-(\d+)\b'), lambda m: ('OQ:' + m.group(1)) if m.group(1) in oq_heading else None))
    pats.append((re.compile(r'\bARCH\b|Technical Architecture v3'), lambda m: 'ARCH'))
    pats.append((re.compile(r'\bVISION\b|Visão Completa do MMORPG'), lambda m: 'VISION'))
    pats.append((re.compile(r'\bEngine (' + '|'.join(E_NAMES) + r')\b'), lambda m: 'E:' + m.group(1)))
    pats.append((re.compile(r'\b(' + '|'.join(E_NAMES) + r') module\b'), lambda m: 'E:' + m.group(1)))
    if note_repo == 'engine':
        pats.append((re.compile(r'\b(' + '|'.join(E_DISTINCT + ['Water', 'Environment', 'Editor']) + r')\b'),
                     lambda m: 'E:' + m.group(1)))
    else:
        pats.append((re.compile(r'\b(' + '|'.join(['GameplayFlow', 'DistributedSimulation']) + r')\b'),
                     lambda m: 'E:' + m.group(1)))
    plat_words = {'ServerBindings': 'ServerBindings', 'CoreServices': 'CoreServices', 'BuildingBlocks': 'BuildingBlocks',
                  'GoSDK': 'GoSDK', 'Go SDK': 'GoSDK', 'ServicesBridgeLayer': 'ServerBindings',
                  'LifecycleLayer': 'ServerBindings', '[Ss]ervices bridge': 'ServerBindings',
                  '[Ss]erver bindings': 'ServerBindings', '[Cc]ore services': 'CoreServices',
                  'World Coordinator': 'CoreServices', '[Ww]orld directives': 'CoreServices',
                  '[Bb]uilding blocks': 'BuildingBlocks', '[Ii]tem ledger': 'BuildingBlocks',
                  '[Cc]urrency wallets': 'BuildingBlocks', 'Platform Transport': 'Transport',
                  'Platform Contracts': 'Contracts', 'Platform Infrastructure': 'Infrastructure'}
    if note_repo == 'platform':
        plat_words.update({'Transport': 'Transport', 'Contracts': 'Contracts', 'Infrastructure': 'Infrastructure'})
    for w, mod in sorted(plat_words.items(), key=lambda kv: -len(kv[0])):
        pats.append((re.compile(r'\b' + w + r'\b'), (lambda mod: lambda m: 'P:' + mod)(mod)))
    if not public:
        pats.append((re.compile(r'\b(' + '|'.join(GAME_AREAS) + r')\b'), lambda m: 'G:' + m.group(1)))
        pats.append((re.compile(r'\b(\w+)Service\b'), lambda m: ('S:' + m.group(1)) if m.group(1) in services else None))
        pats.append((re.compile(r'\b(' + '|'.join(sorted(SVC_PHRASES, key=len, reverse=True)) + r') service\b', re.I),
                     lambda m: SVC_PHRASES.get(m.group(1).lower())))
    return pats

DEP_CELL_NAMES = re.compile(r'\b(' + '|'.join(E_NAMES + ['Contracts', 'Go SDK']) + r')\b')
CODE_PATH = re.compile(r'docs/([\w./-]+\.md)')

# ---------------------------------------------------------------- inline markers
SEG_RE = re.compile(r'(`[^`]*`|!?\[[^\]]*\]\([^)]*\)|\[\[[^\]]*\]\]|\[\^[^\]]+\]|<[^>]+>|https?://\S+)')

def annotate(text, me, src, pats):
    out, used = [], []
    in_fence = False
    seen = set()
    header_cells = None
    for line in text.split('\n'):
        s = line.lstrip()
        if s.startswith('```') or s.startswith('~~~'):
            in_fence = not in_fence
            out.append(line); continue
        if in_fence or s.startswith('#') or s.startswith('<a id') or re.match(r'^\|?\s*:?-{3,}', s):
            if s.startswith('## ') or s.startswith('# '):
                seen = set()
            out.append(line); continue
        is_table = s.startswith('|')
        if is_table and header_cells is None:
            header_cells = [c.strip() for c in s.strip().strip('|').split('|')]
        if not is_table:
            header_cells = None
        dep_cols = set()
        if is_table and header_cells:
            cells = [c.strip() for c in s.strip().strip('|').split('|')]
            dep_cols = {i for i, h in enumerate(header_cells) if h in ('Depends on', 'May depend on')}
            if cells and cells[0] in ('Engine modules',):
                dep_cols = {1}

        def mark(key):
            if key is None or key == me or key in seen:
                return ''
            seen.add(key)
            lab = label_of(key) if key not in ('ARCH', 'VISION') and not key.startswith('OQ:') else \
                ('arch' if key == 'ARCH' else 'vision' if key == 'VISION' else 'oq-' + key[3:])
            if key not in used:
                used.append(key)
            return f'[^{lab}]'

        # split into protected segments and text; track table cell index for dependency columns
        pieces = SEG_RE.split(line)
        cell = -1
        new = []
        for i, piece in enumerate(pieces):
            if i % 2 == 1:   # protected
                new.append(piece)
                if piece.startswith('`'):
                    code = piece[1:-1]
                    add = ''
                    for rx, fn in pats[:2]:
                        for m in rx.finditer(code):
                            add += mark(fn(m))
                    for m in CODE_PATH.finditer(code):
                        k = path_key(m.group(1), src)
                        add += mark(k)
                    new.append(add)
                continue
            # plain text: find matches left to right, non-overlapping across all patterns
            spans = []
            for rx, fn in pats:
                for m in rx.finditer(piece):
                    k = fn(m)
                    if k:
                        spans.append((m.start(), m.end(), k))
            if is_table and dep_cols:
                # bare names inside dependency columns
                pos = 0
                for part_idx, part in enumerate(piece.split('|')):
                    cidx = cell + part_idx
                    if cidx in dep_cols:
                        for m in DEP_CELL_NAMES.finditer(part):
                            nm = m.group(1)
                            k = 'P:Contracts' if nm == 'Contracts' else 'P:GoSDK' if nm == 'Go SDK' else 'E:' + nm
                            spans.append((pos + m.start(), pos + m.end(), k))
                    pos += len(part) + 1
                cell += piece.count('|')
            elif is_table:
                cell += piece.count('|')
            spans.sort(key=lambda t: (t[0], -(t[1] - t[0])))
            res, last = [], 0
            for a, b, k in spans:
                if a < last:
                    continue
                # do not split a word: extend end over trailing word characters
                res.append(piece[last:b])
                res.append(mark(k))
                last = b
            res.append(piece[last:])
            new.append(''.join(res))
        out.append(''.join(new))
    return '\n'.join(out), used

def path_key(docpath, src):
    repo = src['repo']
    if repo == 'system':
        if docpath.startswith('system/'):
            return 'D:engine:' + docpath if ('engine', docpath) in files_set else None
        return None
    k = doc_key(repo, docpath)
    if k in notes:
        return k
    m = re.match(r'(modules|services)/(\w+)\.md', docpath)
    if m:
        for p in ('E', 'P', 'G'):
            if m.group(1) == 'modules' and p + ':' + m.group(2) in notes and notes[p + ':' + m.group(2)]['repo'] == repo:
                return p + ':' + m.group(2)
        if m.group(1) == 'services' and m.group(2).endswith('Service') and 'S:' + m.group(2)[:-7] in notes:
            return 'S:' + m.group(2)[:-7]
    return None

files_set = set(files)

# ---------------------------------------------------------------- per-note metadata and connections
def note_info(repo, docrel):
    """Key, tags, aliases and structural connections of one note."""
    tags, aliases, conn = [], [], []    # conn: list of (title, [keys])
    system = docrel.startswith('system/')
    if not system:
        tags.append(f'repo/{repo}')
    tags.append('source/arch')
    if repo == 'game' and not system:
        tags.append('source/vision')
    key = None
    hubs = []
    if system:
        tags += ['type/system', 'mirror/system']
        base = docrel[7:-3]
        if base == 'glossary':
            tags.append('type/glossary')
        key = doc_key('engine', docrel)   # self-identity for self-skip (path resolved per copy)
        hubs = ['D:engine:system/README.md', 'D:engine:system/glossary.md']
        if base in ('03-dependency-rules', '12-work-packages', '08-implementation-roadmap', '02-module-contract'):
            conn.append(('Engine modules', ['E:' + n for n in engine]))
            conn.append(('Platform modules', ['P:' + n for n in platform]))
            conn.append(('Game', ['GP']))
    elif docrel.startswith('modules/') and docrel != 'modules/README.md':
        name = docrel[8:-3]
        if repo == 'engine':
            key = 'E:' + name
            e = engine[name]
            tags += ['type/module', f'group/{e["group"]}', f'headless/{e["headless"]}']
            if e['slice']:
                tags.append(f'slice/{e["slice"]}')
            aliases.append(f'Runic::{name}')
            deps = ['E:' + d for d in (e['allows'] or [])]
            conn.append(('Depends on', deps if e['allows'] is not None else []))
            if e['allows'] is None:
                conn.append(('Depends on', ['E:' + n for n in engine if n not in ('Editor',)]))
            conn.append(('Depended on by', ['E:' + n for n, x in engine.items() if x['allows'] and name in x['allows']]
                         + ['P:' + p for p, d in PLATFORM_DEPS.items() if key in d]))
            hubs = ['D:engine:modules/README.md', 'D:engine:system/03-dependency-rules.md',
                    'D:engine:system/12-work-packages.md', 'D:engine:current-state.md']
        elif repo == 'platform':
            key = 'P:' + name
            tags += ['type/module', f'lang/{PLATFORM_LANG[name]}']
            if platform[name]['slice']:
                tags.append(f'slice/{platform[name]["slice"]}')
            aliases += PLATFORM_ALIASES.get(name, [])
            conn.append(('Depends on', PLATFORM_DEPS[name]))
            conn.append(('Depended on by', ['P:' + p for p, d in PLATFORM_DEPS.items() if key in d]))
            hubs = ['D:platform:modules/README.md', 'D:platform:system/03-dependency-rules.md',
                    'D:platform:system/12-work-packages.md', 'D:platform:current-state.md']
        else:
            key = 'G:' + name
            ga = game_areas[name]
            tags += ['type/game-area', f'target/{ga["target"]}']
            if re.match(r'\d', ga['slice']):
                tags.append(f'slice/{ga["slice"]}')
            hubs = ['D:game:modules/README.md', 'D:game:game-packages.md', 'D:game:architecture.md',
                    'D:game:current-state.md']
    elif docrel.startswith('services/') and docrel != 'services/README.md':
        name = docrel[9:-10]
        key = 'S:' + name
        s = services[name]
        tags += ['type/service', 'lang/go']
        if re.match(r'\d', s['slice']):
            tags.append(f'slice/{s["slice"]}')
        aliases.append(f'{s["label"]} service')
        hubs = ['D:game:services/README.md', 'D:game:game-packages.md', 'D:game:architecture.md',
                'D:game:current-state.md']
    else:
        key = doc_key(repo, docrel)
        t = {'README.md': 'index', 'architecture.md': 'architecture', 'current-state.md': 'current-state',
             'modules/README.md': 'catalog', 'services/README.md': 'catalog', 'game-packages.md': 'package-list',
             'build/BUILD-1.md': 'build-contract'}.get(docrel, 'doc')
        tags.append(f'type/{t}')
        if docrel == 'modules/README.md':
            pre = {'engine': 'E', 'platform': 'P', 'game': 'G'}[repo]
            conn.append(('Catalog', [k for k in notes if k.startswith(pre + ':')]))
        if docrel == 'services/README.md':
            conn.append(('Catalog', [k for k in notes if k.startswith('S:')]))
        if docrel == 'game-packages.md':
            conn.append(('Package homes', sorted({v for v in game_home.values()})))
        if docrel == 'README.md':
            conn.append(('Start here', [k for k in notes if k.startswith(f'D:{repo}:') and k != key]))
        hubs = [f'D:{repo}:README.md', f'D:{repo}:system/README.md']
    # package order (modules, areas, services)
    if key and key[0] in 'EPGS' and not system:
        own = owned_packages(key)
        public = repo in PUBLIC
        needs = []
        for p in own:
            for n in pkg_needs.get(p, []):
                k = pkg_owner(n, repo, public)
                if k != key and k not in needs:
                    needs.append(k)
        needed_by = []
        for p, ns in pkg_needs.items():
            if any(pkg_owner(n, repo, public) == key for n in ns):
                k = pkg_owner(p, repo, public)
                if k != key and k not in needed_by:
                    needed_by.append(k)
        if own:
            conn.append(('Its packages need', needs))
        conn.append(('Packages that need it', needed_by))
    conn.append(('See also', [h for h in hubs if h != key]))
    return key, tags, aliases, conn

# ---------------------------------------------------------------- render
def rooted(k, system):
    return system and (k == 'GP' or (len(k) > 1 and k[0] in 'EPGS' and k[1] == ':'))

def footnote_def(k, src_rel, src):
    if k == 'ARCH':
        return '#source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)'
    if k == 'VISION':
        return '#source/vision — VISION, "RunicGame — Visão Completa do MMORPG": the game design source of truth (a project document, not a note in this vault)'
    if k.startswith('OQ:'):
        target = posixpath.join(src['vault_root'], 'docs', 'system/11-open-questions')
        rel = wikilink(src_rel, target, 'x')[2:-4]
        return f'[[{rel}#{oq_heading[k[3:]]}|OQ-{k[3:]}]] — open question or decision'
    t, name = resolve(k, src)
    return f'{wikilink(src_rel, t, name, system_src=rooted(k, src["system"]))} — {describe(k)}'

def process(repo, docrel):
    text = read(repo, docrel)
    system = docrel.startswith('system/')
    src_rel = vault(repo, docrel)
    src = dict(repo='system' if system else repo, system=system, vault_root=REPOS[repo][1])
    key, tags, aliases, conn = note_info(repo, docrel)
    public = system or repo in PUBLIC
    pats = build_patterns('system' if system else repo, public)
    me = key
    body, used = annotate(text, me, src, pats)
    # frontmatter
    fm = ['---', FM_MARK, 'tags:'] + [f'  - {t}' for t in tags]
    if aliases:
        fm += ['aliases:'] + [f'  - "{a}"' for a in aliases]
    fm.append('---')
    # connections
    lines = ['', START, '', '---', '', '## Connections', '',
             '*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*', '']
    seen = set()
    for title, keys in conn:
        ks = []
        for k in keys:
            if k and k != me and k not in seen and k not in ks:
                ks.append(k)
        if not ks:
            continue
        seen.update(ks)
        links = ' · '.join(wikilink(src_rel, *resolve(k, src), system_src=rooted(k, system)) for k in ks)
        lines.append(f'- **{title}:** {links}')
    lines.append('')
    for k in used:
        lab = 'arch' if k == 'ARCH' else 'vision' if k == 'VISION' else ('oq-' + k[3:]) if k.startswith('OQ:') else label_of(k)
        lines.append(f'[^{lab}]: {footnote_def(k, src_rel, src)}')
    out = '\n'.join(fm) + '\n\n' + body.rstrip('\n') + '\n' + '\n'.join(lines).rstrip('\n') + '\n'
    return out

changed = 0
for repo, docrel in files:
    new = process(repo, docrel)
    path = os.path.join(REPOS[repo][0], 'docs', docrel)
    old = open(path, encoding='utf-8').read()
    if new != old:
        changed += 1
        if WRITE:
            open(path, 'w', encoding='utf-8').write(new)
print(f'notes: {len(files)}, changed: {changed}, write: {WRITE}')
