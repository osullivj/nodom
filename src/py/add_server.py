# std pkg
import asyncio
import json
import logging
# 3rd pty
from tornado.options import options, parse_command_line, define
# nodom
import nd_web
import nd_utils

NDAPP='add_server'

logr = nd_utils.init_logging(NDAPP, console=True)

ADDITION_LAYOUT = [
    dict(rname="Home",
        cspec=dict(title='Server side addition', font='Arial'),
        children=[
            dict(rname="PushFont", cspec=dict(font='Courier')),
            dict(rname='InputInt', cspec=dict(cname='op1', step=1)),
            dict(rname='InputInt', cspec=dict(cname='op2', step=2)),
            # see src/imgui.ts for enum defns
            # InputTextFlags.ReadOnly == 1 << 14 == 16384
            dict(rname='InputInt', cspec=dict(cname='op1_plus_op2', flags=16384)),
            dict(rname='Separator', cspec=dict()),
            dict(rname='PopFont'),
            dict(rname='PushFont', cspec=dict(font='Courier', font_size_base=0.5)),
            dict(rname='Footer', cspec=dict(db=True, fps=True, demo=True, id_stack=True, memory=True)),
            dict(rname='PopFont'),
        ],
    ),
]


ADDITION_DATA = dict(
    home_title = 'WebAddition',
    op1=2,
    op2=3,
    op1_plus_op2=5,
)

is_operand_change = lambda c: c.get('cache_key') in ['op1', 'op2']

class AdditionService(nd_utils.Service):
    def on_client_data_change(self, uuid, client_change):
        logr.info(f'on_client_data_change:client:{uuid}, change:{client_change}')
        if is_operand_change(client_change):
            # op1 or op2 has changed: so recalc the sum
            ckey = 'op1_plus_op2'
            data_cache = self.cache['data']
            logr.info(f'on_data_change: data_cache:{data_cache}')
            logr.info(f'op1:{data_cache['op1']}, op2:{data_cache['op2']}')
            new_val = data_cache['op1'] + data_cache['op2']
            change = dict(nd_type='DataChange', old_value=data_cache[ckey], new_value=new_val, cache_key=ckey)
            data_cache[ckey] = new_val
            return [change]


define("port", default=8890, help="run on the given port", type=int)

# breadboard looks out for service at the module level
service = AdditionService(NDAPP, ADDITION_LAYOUT, ADDITION_DATA)

async def main():
    parse_command_line()
    app = nd_web.NDApp(service)
    app.listen(options.port)
    logr.info(f'{NDAPP} port:{options.port}')
    await asyncio.Event().wait()

if __name__ == "__main__":
    asyncio.run(main())
