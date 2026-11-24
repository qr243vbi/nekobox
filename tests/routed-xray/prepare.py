"""Test the production finalization step without GUI, profiles or processes."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tests'))
from test_link_tls import function

builder = (root / 'src/gharqad/configs/ConfigBuilder.cpp').read_text(encoding='utf-8')
finalize = function(builder, 'static void FinalizeBuiltinXray(')
# This signature has {} default arguments. Start at the body after the closing
# parameter list rather than treating the first default initializer as its body.
start = builder.index('static void AddTunRuntimeRules(')
brace = builder.index(') {', start) + 2
depth = 0
for end in range(brace, len(builder)):
    depth += (builder[end] == '{') - (builder[end] == '}')
    if depth == 0:
        add_tun = builder[start:end + 1]
        break
assert 'patchedRules = transportRules' in add_tun
build = function(builder, 'BuildConfig(const std::shared_ptr<ProxyEntity> &ent, bool forTest,')
assert build.rfind('FinalizeBuiltinXray(status)') > build.rfind('BuildConfigSingBox(status)')
outbound = function(builder, 'void BuildOutbound(')
assert 'runtime.addProfile(tag, config)' in outbound
chain = function(builder, 'QString BuildChainInternal(')
assert 'xrayRuntime.chainThrough(previousXrayTag' in chain
fixture = (Path(__file__).parent / 'finalize.cpp.in').read_text(encoding='utf-8')
merge_start = builder.index('    auto routeRules = status->xrayRuntime.transportRules;')
merge_end = builder.index('    // tun process routing', merge_start)
merge_rules = builder[merge_start:merge_end]
header = (root / 'src/nekobox/configs/ConfigBuilder.hpp').read_text(encoding='utf-8')
types = function(header, 'class ExtraCoreData') + ';\n' + function(header, 'class BuildConfigResult') + ';\n'
Path(sys.argv[1]).write_text(fixture.replace('@FINALIZE@', finalize).replace('@ADD_TUN@', add_tun)
                           .replace('@RESULT_TYPES@', types).replace('@MERGE_RULES@', merge_rules), encoding='utf-8')
