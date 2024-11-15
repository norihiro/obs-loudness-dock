#! /usr/bin/env python3

'''
This tool watches loudness and report to Discord.
'''

# pylint: disable=W0702 # This script should try not to terminate at errors.

import asyncio
import argparse
import os
import sys
from datetime import datetime, timedelta
import discord
import simpleobsws


class MyOBSWSClient:
    'Client class to communicate with obs-websocket'
    def __init__(self, args):
        self.last_exceed = None
        self.args = args
        self.obsws = None

    async def ws_connect(self):
        'Connect to obs-websocket'
        try:
            obsws = simpleobsws.WebSocketClient(
                    url=f'ws://{self.args.obsws}',
                    password=self.args.obsws_passwd)
            await obsws.connect()
            await obsws.wait_until_identified()
            self.obsws = obsws
        except:
            print(f'Info: {self.args.obsws}: Connection failed. Will retry later')

    async def ws_send_request(self, req, data=None, retry=2):
        'Send a request to obs-websocket'
        while retry > 0:
            if not self.obsws:
                await self.ws_connect()

            try:
                res = await self.obsws.call(simpleobsws.Request(req, data))
                if res.ok():
                    return res.responseData
            except:
                if self.obsws:
                    await self.obsws.disconnect()
                    self.obsws = None

            retry -= 1

    async def check_alert_cond(self):
        '''
        Check the condition for if_streaming and if_recording
        If both if_streaming and if_recording are set,
        the alert is disabled only if both streaming and recording are inactive.
        '''

        if self.args.if_streaming:
            res = await self.ws_send_request('GetStreamStatus')
            if res and res['outputActive']:
                return True

        if self.args.if_recording:
            res = await self.ws_send_request('GetRecordStatus')
            if res and res['outputActive']:
                return True

        if self.args.if_streaming or self.args.if_recording:
            return False

        return True

    async def run_loudness_watcher(self, cb):
        'A loop method checking the loudness'

        time_threshold = timedelta(seconds=self.args.wait_time)

        if not self.obsws:
            await self.ws_connect()

        while True:
            await asyncio.sleep(3)

            if not await self.check_alert_cond():
                self.last_exceed = datetime.now()
                continue

            res = await self.ws_send_request(
                    'CallVendorRequest',
                    {
                        'vendorName': 'obs-loudness-dock',
                        'requestType': 'get_loudness',
                        'requestData': {}
                    })
            if res:
                short_lufs = res['responseData']['short']

                too_low = False
                low_duration_s = 0

                if short_lufs < self.args.threshold and self.last_exceed:
                    td = datetime.now() - self.last_exceed
                    if td > time_threshold:
                        too_low = True
                        low_duration_s = td.total_seconds()

                else:
                    self.last_exceed = datetime.now()

                await cb(too_low=too_low, short_lufs=short_lufs, low_duration_s=low_duration_s)


class MyClient(discord.Client):
    'Discord client that notify too low loudness'

    def __init__(self, args):
        intents = discord.Intents.default()
        intents.message_content = True
        super().__init__(intents=intents)

        self.args = args
        self.n_too_low = 0
        self.msg_too_low = None
        self.msg_current = None
        self.channel = None
        self._on_ready_called = False

    async def on_ready(self):
        'Callback method from discord.Client'

        if self._on_ready_called:
            return
        self._on_ready_called = True

        print(f'Info: Logged on as "{self.user}"')
        obsws = MyOBSWSClient(self.args)
        self.loop.create_task(obsws.run_loudness_watcher(self.on_loudness))

        if self.args and self.args.list_channels:
            for ch in self.get_all_channels():
                print(ch)

        if self.args and self.args.channel:
            print(f'Info: Searching channel {self.args.channel}')
            found = False
            for ch in self.get_all_channels():
                if ch.name == str(self.args.channel):
                    self.channel = ch
                    found = True
                    break
            if not found:
                await self.close()
                raise ValueError(f'channel "{self.args.channel}" not found')

        if self.args and self.args.send_text:
            print(f'Info: Sending text: "{self.args.send_text}"')
            await self.channel.send(content = self.args.send_text)

    async def on_loudness(self, too_low=False, short_lufs=None, low_duration_s=None):
        'Callback method when the loudness arrives'
        # pylint: disable=R0912 # Fix it someday later.

        threshold_str = f'{self.args.threshold:0.1f} LUFS'
        duration_str = f'{int(low_duration_s)} s'
        short_str = f'{short_lufs:+0.1f} LUFS'

        if too_low:
            if self.msg_current:
                await self.msg_current.delete()
                self.msg_current = None

            text = f'Too low loudness (< {threshold_str}) continues for {duration_str}.\n' + \
                   f'Current loudness: {short_str}'
            if self.n_too_low == 0:
                self.msg_too_low = await self.channel.send(content = text)
            elif self.msg_too_low:
                try:
                    await self.msg_too_low.edit(content = text)
                except:
                    self.msg_too_low = None

        else:
            if self.msg_too_low:
                text = self.msg_too_low.content + \
                       f'\n--> Resolved. Current loudness: {short_str}'
                try:
                    await self.msg_too_low.edit(content = text)
                except:
                    self.msg_too_low = None

            if self.args.message_current:
                text = f'Current loudness: {short_str}'
                if self.msg_current:
                    try:
                        await self.msg_current.edit(content = text)
                    except:
                        self.msg_current = None
                else:
                    self.msg_current = await self.channel.send(content = text)

        if too_low:
            self.n_too_low += 1
        else:
            self.n_too_low = 0


def _default_token():
    'Returns the default token file path'
    if sys.platform == 'linux':
        return os.getenv('HOME')+'/.config/discordbot/token'
    if sys.platform == 'darwin':
        return os.getenv('HOME')+'/Library/Application Support/discordbot/token'
    return '.token'


def get_args():
    'Parse the arguments'

    parser = argparse.ArgumentParser(
			description = 'Loudness watch tool reporting to Discord',
			formatter_class=argparse.ArgumentDefaultsHelpFormatter,
			)

    # arguments for Discord
    parser.add_argument('--list-channels', action='store_true', help='list channels')
    parser.add_argument('--channel', nargs='?', help='channel name')
    parser.add_argument('--send-text', nargs='?', help='send text to channel')
    parser.add_argument('--token', dest='token_file', nargs='?',
                        help='file path that contains the token',
                        default=_default_token())

    # arguments for obs-websocket
    parser.add_argument('--obsws', action='store', default='127.0.0.1:4455',
                        help='obs-websocket host and port, separated by colon(:)')
    parser.add_argument('--obsws-passwd', action='store', default=None,
                        help='Password for obs-websocket')

    # arguments for loudness check
    parser.add_argument('--threshold', action='store', type=float, default=-36.0,
                        help='Threshold in LUFS')
    parser.add_argument('--wait-time', action='store', type=float, default=10.0,
                        help='Time in second that the lower loudness continues for')
    parser.add_argument('--if-streaming', action='store_true', default=False,
                        help='Alert only while streaming')
    parser.add_argument('--if-recording', action='store_true', default=False,
                        help='Alert only while recording')

    # arguments for messaging verbosity
    parser.add_argument('--message-current', action='store_true', default=False,
                        help='Send current loudness')

    args = parser.parse_args()
    if args.send_text and args.send_text=='-':
        args.send_text = sys.stdin.read()
    return args


def main():
    'Main routine'

    args = get_args()

    with open(args.token_file, encoding='ascii') as f:
        token = f.read().strip()

    client = MyClient(args=args)
    client.run(token)


if __name__ == '__main__':
    main()
