# -*- coding: utf-8 -*-
"""Public tutorial and Web Bluetooth links requested by the project author."""
import asyncio

SITE = 'https://midea.00571.top/'
BLE = SITE + '#provision'
WEB_SEGMENTS = [
    dict(slot=49, kind='overview', title='配套网页教程与蓝牙配网', chapter='网页教程与配网入口',
         narration='配套的图文教程和蓝牙配网，已经放在屏幕上的网址。用浏览器打开这个地址，顶部可以在安装与刷写教程、蓝牙配网之间切换。接线、刷机、配网和设置，都能按网页目录找到对应步骤。'),
    dict(slot=50, kind='ble', title='打开线上蓝牙配网页面', chapter='04 联网与蓝牙配网',
         narration='需要蓝牙配网时，在浏览器中打开屏幕上的网址。先点击顶部蓝牙配网，再点击连接蓝牙设备。安卓可以用 Chrome，Windows 可以用 Chrome 或 Edge；苹果手机使用支持网页蓝牙的 Bluefy。接下来，我们按连接设备、验证密码、选择网络和保存重启的顺序操作。'),
    dict(slot=51, kind='outro', title='图文教程与蓝牙配网，随时打开', chapter='图文教程与蓝牙配网网址',
         narration='完整的图文教程和蓝牙配网，都在屏幕上的网站。用浏览器打开网址，就能查看对应页面。需要回看接线、刷机或者设置，按网页目录查找就可以。我们下期见。')
]

async def main():
    import synthesize_natural as synth
    # The renderer's original 49 scenes remain intact on disk.
    synth.SCENES = synth.SCENES[:49] + WEB_SEGMENTS
    semaphore = asyncio.Semaphore(2)
    await asyncio.gather(*(synth.synthesize(s['slot'], s, semaphore) for s in WEB_SEGMENTS))
    await asyncio.sleep(.25)

if __name__ == '__main__':
    asyncio.run(main())
