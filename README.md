# FxPresser
这是一个在游戏中自动释放技能(按F1-F10)的程序，与目前广泛流传的"魔手"是同一类工具。

## 目的
在使用"魔手"时感觉有一些不方便的地方，此程序在"魔手"功能基础上加入一些改进。

## 亮点
- 全局开关功能，实现一键启停。
- 自动保存/读取参数，包括游戏的角色名/按键启用状态/按键间隔/自定义的窗口标题等等，免去重复操作。
- 可以随意调节按键间隔(0.1-365.0秒)。提供一位小数便于更精确的控制。
- 修改窗口标题的功能，方便各种直播软件识别/自动切换多个游戏窗口。

## 使用
### 启动
确保已经登录游戏角色，且游戏窗口已经**最大化**。双击FxPresser.exe启动程序。</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/main.png)</br></br>

---
### 扫描游戏窗口
程序启动时会自动扫描一次游戏窗口。如果是后登录的游戏，点击**扫描游戏窗口**按钮。程序会寻找FFO游戏窗口，并将角色名截图放到下方下拉列表中。截图不成功(如被最小化)的游戏窗口将不被包含。</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/scanned.png)</br></br>

---
### 选择窗口
在下拉列表中选择需要控制的角色名。</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/selected.png)</br></br>

---
### 设置参数
1. **窗口标题**</br>
随意填写，留空则不会修改窗口标题。</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/set_title.png)</br></br>
然后点击**修改窗口标题**按钮，即可看到效果</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/renamed_title.png)</br></br>
2. **全局间隔**</br>
此间隔是任意两个按键触发的最小间隔，后来的按键会被推迟至少这么长的时间。防止多个按键都要触发的时候，游戏中却把后来的技能卡掉。根据携带武器的施法衔接速度，在游戏中试出合适的值。</br></br>
3. **F1-F10的参数设置**</br>
以**刺客**常用的一套挂机技能为例</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/skill_sample.png)</br></br>
参数示例</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/arg_sample.png)</br></br>
**间隔**的数值大约为debuff持续时间/CD+施法时间，达到技能效果消失立马接上的效果。注意不能直接设置成CD时间，因为按下按键之后还有一个施法动作才会开始计算CD。当然像"魔手"那样全部设置成1秒也是可以的，此时要把**全局间隔**调得足够小，否则后面的技能只能一直处于推迟状态而无法触发。</br></br>
**间隔**参数只有在不勾选**启用**的时候才能编辑</br></br>
**是缺省技能**标识此按键对应的技能是否为游戏中的缺省技能，最多只能有一个按键被设为缺省。被设为缺省的按键只会被触发一次，所以**间隔**参数是无效的。**此选项建议只用于平砍**。</br></br>

---
### 使用
1. 要开始挂机打怪的时候，点击**全局开关**，令其处于勾选状态。所有**启用**的按键会立即被触发一次，被设置为**缺省技能**的按键会首先被触发，剩余的技能按顺序触发。在此过程中可以观察**全局间隔**设置是否正确(不卡技能为准)。</br></br>
2. 打怪完毕，再点击一次**全局开关**，令其处于不勾选状态即可。</br></br>
3. 程序退出时会保存所有相关参数，下次使用时只要先登录角色再打开程序，即可自动完成定位窗口+修改标题的工作，达到只需要点击**全局开关**的状态。

---
### 多开
- 由于自动定位窗口功能，我并没有设计在程序界面中切换参数，多开通过复制多份FxPresser.exe完成。实际上程序的文件名可以任取，每个exe都会有自己的一份参数。</br>
![](https://raw.githubusercontent.com/ClansChen/MDPic/main/FxPresser/multi_instance.png)</br></br>

---
### 注意事项
- 程序是对游戏画面的固定区域进行截图，为了自动寻找窗口能正确工作，建议先登录游戏角色，并展开角色头像，再启动本程序。

- 此程序触发的按键会跟真实键盘的按键形成组合键，所以在程序工作时不要在游戏中随意按Alt，以免误触发其他技能/物品。同时按键对应的Alt组合键位置最好**不要**放其他技能或物品。
