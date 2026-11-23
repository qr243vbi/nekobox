"""Compile the production bypass-rule helpers with Qt; no application or TUN."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]

def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end]

entity = (root / 'src/gharqad/dataStore/RouteEntity.cpp').read_text(encoding='utf-8')
builder = (root / 'src/gharqad/configs/ConfigBuilder.cpp').read_text(encoding='utf-8')
factory = function(entity, 'std::shared_ptr<RouteRule> RouteRule::get_processPath_direct_rule(')
prepend = function(builder, 'static void PrependExtraCoreBypassRule(')
assert builder.count('PrependExtraCoreBypassRule(routeChain, status->result->extraCoreData->path);') == 2
fixture = (Path(__file__).parent / 'fixture.cpp.in').read_text(encoding='utf-8')
Path(sys.argv[1]).write_text(fixture.replace('@FACTORY@', factory).replace('@PREPEND@', prepend), encoding='utf-8')
