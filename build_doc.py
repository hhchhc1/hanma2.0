#!/usr/bin/env python3
"""基于 moban.docx 模板生成设计文档 — 精准清理 + 重写正文"""

from docx import Document
from docx.shared import Pt, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
import os

TEMPLATE = 'D:/moban.docx'
OUTPUT   = 'D:/msjhhc/comprehensive_routine_70inch_mipilcd/悍马2.0_物联网设计竞赛_设计文档.docx'

PROJECT  = '悍马2.0—基于ESP32-P4的智能语音AI管家'
SCHOOL   = '中北大学'
TEAM     = '悍马组合'
LEADER   = '韩皓丞'
M1 = '马世杰'
M2 = '姓名'
M3 = '姓名'

doc = Document(TEMPLATE)

# ═══════════════════════════════════════════
# Helpers
# ═══════════════════════════════════════════
def set_run_font(run, cn='宋体', en='Times New Roman', size=Pt(12), bold=False):
    run.font.name = en
    run._element.rPr.rFonts.set(qn('w:eastAsia'), cn)
    run.font.size = size
    run.bold = bold

def body(text, indent=True):
    p = doc.add_paragraph()
    p.style = doc.styles['Normal']
    if indent:
        p.paragraph_format.first_line_indent = Pt(24)
    else:
        p.paragraph_format.first_line_indent = Pt(0)
    p.paragraph_format.line_spacing = 1.5
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    run = p.add_run(text)
    set_run_font(run, '宋体', 'Times New Roman', Pt(12))
    return p

def add_h1(text):
    h = doc.add_heading(text, level=1)
    h.alignment = WD_ALIGN_PARAGRAPH.CENTER
    for run in h.runs:
        set_run_font(run, '黑体', 'Times New Roman', Pt(16), True)
    h.paragraph_format.line_spacing = 1.5
    h.paragraph_format.space_before = Pt(6)
    h.paragraph_format.space_after = Pt(6)
    return h

def add_h2(text):
    h = doc.add_heading(text, level=2)
    for run in h.runs:
        set_run_font(run, '黑体', 'Times New Roman', Pt(14), True)
    h.paragraph_format.line_spacing = 1.5
    h.paragraph_format.space_before = Pt(6)
    h.paragraph_format.space_after = Pt(3)
    return h

def add_h3(text):
    h = doc.add_heading(text, level=3)
    for run in h.runs:
        set_run_font(run, '黑体', 'Times New Roman', Pt(12), True)
    h.paragraph_format.line_spacing = 1.5
    h.paragraph_format.space_before = Pt(4)
    h.paragraph_format.space_after = Pt(2)
    return h

def add_ref(text):
    p = doc.add_paragraph()
    p.style = doc.styles['Normal']
    p.paragraph_format.first_line_indent = Pt(0)
    p.paragraph_format.line_spacing = 1.5
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    run = p.add_run(text)
    set_run_font(run, '宋体', 'Times New Roman', Pt(10.5))
    return p

# ═══════════════════════════════════════════
# Step 1: Fill cover table
# ═══════════════════════════════════════════
cover_table = doc.tables[0]
fields = {
    (0,1): SCHOOL,
    (1,1): TEAM,
    (3,1): LEADER,
    (4,1): M1,
    (5,1): M2,
    (6,1): M3,
}
for (r,c), val in fields.items():
    cell = cover_table.cell(r, c)
    for p in cell.paragraphs:
        for run in p.runs:
            run.text = ''
    p = cell.paragraphs[0]
    run = p.add_run(val)
    set_run_font(run, '宋体', 'Times New Roman', Pt(14))

# Fill project name in Title style paragraph
for p in doc.paragraphs:
    if p.style.name == 'Title':
        for run in p.runs:
            run.text = ''
        p.runs[0].text = PROJECT
        set_run_font(p.runs[0], '黑体', 'Times New Roman', Pt(16), True)
        break

# Fill project name in "设计作品名称" placeholder on body page
for p in doc.paragraphs:
    if p.text.strip() == '设计作品名称':
        for run in p.runs:
            run.text = ''
        p.runs[0].text = PROJECT
        set_run_font(p.runs[0], '黑体', 'Times New Roman', Pt(16), True)

# ═══════════════════════════════════════════
# Step 2: Replace abstract
# ═══════════════════════════════════════════
abstract_paras = [
    '本作品"悍马2.0"是一款基于乐鑫ESP32-P4芯片的智能语音AI管家，采用ESP32-P4（主控）+ESP32-C6（WiFi通信协处理）+ESP32-C3（远程传感器节点）的异构分布式架构。系统以"你好小智"为语音唤醒词，支持用户通过自然语言控制灯光与风扇，实时查询温湿度及光照数据。7寸MIPI触摸屏提供智能家居控制面板和微信风格对话气泡界面，MIPI CSI摄像头支持人脸检测自动解锁，屏幕背光根据环境光照自动调节。远程ESP32-C3节点通过mDNS组播DNS自动发现P4主机（xiaozhi.local），实现分布式感知即插即用，彻底消除了DHCP动态IP场景下硬编码失效的运维痛点。',
    '在软件架构层面，本作品实现了三项原创性创新。第一，POSIX标准化线程模型：将项目中全部68个源文件的裸FreeRTOS API封装为统一的POSIX标准接口，自研posix_compat模块提供thread、queue、event_group、timer四套标准抽象，上层应用代码具备跨RTOS（NuttX/Zephyr）可移植性，替换底层仅需改动4个头文件。第二，内存隔离保护机制：自研5域分区内存池配合魔数哨兵校验和RISC-V PMP硬件写保护三道防线，将野指针越界写入从"静默破坏、随机死机"变为"即时捕获、精准告警"，从根本上杜绝FreeRTOS扁平内存模型的固有问题。第三，类Matter协议设备模型：采用Endpoint→Cluster→Attribute三层标准架构管理所有智能家居设备，MCP通用工具可自动适配任意新增设备而无需手写接口，Web API通过GET /api/matter/descriptor端点输出完整设备树JSON，为未来对接真实Matter认证设备和Apple HomeKit、Google Home等生态系统预留了标准化升级通道。',
    '作品基于ESP-IDF v5.5.1框架开发，采用LVGL v8.4.0图形库、Opus低延迟语音编码、ESP-ADF音频前端处理、PPA硬件加速缩放和MQTT+WebSocket双通道云端通信等技术栈。系统已实现完整的OTA远程固件升级、云端AI语音对话、Web智能家居面板、摄像头实时推流等功能，在嵌入式系统架构设计、分布式传感器网络和物联网设备标准化方面进行了有意义的探索与实践。',
]

# Find the paragraph range for abstract (between "摘要" H1 and "关键词")
abs_start = None
abs_end = None
for i, p in enumerate(doc.paragraphs):
    if p.style.name == 'Heading 1' and p.text.strip() == '摘要':
        abs_start = i
    if abs_start is not None and '关键词' in p.text:
        abs_end = i
        break

if abs_start and abs_end:
    # Remove old abstract paragraphs between H1 and keywords
    remove_indices = list(range(abs_start + 1, abs_end))
    for idx in reversed(remove_indices):
        el = doc.paragraphs[idx]._element
        el.getparent().remove(el)

    # Insert new abstract before keywords paragraph
    keywords_el = doc.paragraphs[abs_start + 1]._element  # now the keywords para is right after H1
    for ap in reversed(abstract_paras):
        new_p = doc.add_paragraph()
        new_p.style = doc.styles['Normal']
        new_p.paragraph_format.first_line_indent = Pt(24)
        new_p.paragraph_format.line_spacing = 1.5
        new_p.paragraph_format.space_before = Pt(0)
        new_p.paragraph_format.space_after = Pt(0)
        new_p.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
        run = new_p.add_run(ap)
        set_run_font(run, '宋体', 'Times New Roman', Pt(12))
        # Move before keywords
        keywords_el.addprevious(new_p._element)

# ═══════════════════════════════════════════
# Step 3: Remove template chapter content (旧的第一章~参考文献)
# ═══════════════════════════════════════════
# Find the "第一章" H1 by checking paragraphs near index 47
# The template has H1 at these indices (from analysis): 摘要, 设计需求分析, 特色与创新, 功能设计, 系统实现, 其他内容, 参考文献
# We need to find the first chapter H1 and delete from there to end of document body

# Strategy: find all H1 paragraphs, keep "摘要" and "目录", delete the rest (old chapters)
old_chapter_start = None
old_chapter_end = None

# Count through H1s
h1_hits = []
for i, p in enumerate(doc.paragraphs):
    if p.style.name == 'Heading 1':
        h1_hits.append((i, p.text))

# The template H1s (indices below ~130) are old. New ones are 130+.
# Find the first old chapter H1: it's the second H1 (first is 摘要, second is 第一章)
if len(h1_hits) >= 2:
    for idx, text in h1_hits:
        if text.strip() == '摘要':
            continue
        if idx < 130:  # old template content
            if old_chapter_start is None:
                old_chapter_start = idx
            old_chapter_end = idx

# Delete from old_chapter_start to the end (before we add new content)
# Actually, delete from old_chapter_start to the paragraph before the first new H1
total_paras = len(doc.paragraphs)
if old_chapter_start:
    # Remove all template body paragraphs from old_chapter_start to end
    while len(doc.paragraphs) > old_chapter_start:
        last_el = doc.paragraphs[-1]._element
        last_el.getparent().remove(last_el)

# Remove template cloud computing table (Table 1)
if len(doc.tables) > 1:
    tbl = doc.tables[1]
    tbl._element.getparent().remove(tbl._element)

print(f'Cleaned template content. Remaining paragraphs: {len(doc.paragraphs)}')

# ═══════════════════════════════════════════
# Step 4: Write chapters
# ═══════════════════════════════════════════

# ── 第一章 设计需求分析 ──
add_h1('第一章  设计需求分析')
body('本章分析了当前智能家居语音交互领域的主要痛点，以及嵌入式物联网开发面临的可移植性和协议标准化困境，阐述本作品的设计动机和解决目标。')

add_h2('1.1  智能家居语音交互的行业现状与痛点')
body('随着人工智能和物联网技术的飞速发展，智能家居市场持续快速增长。然而，当前主流的智能音箱和语音助手产品普遍存在以下突出问题。第一，设备严重依赖云端处理，语音数据必须上传至远端服务器进行识别和推理，一旦网络中断则设备完全无法使用，用户体验极差。第二，不同品牌之间协议互不兼容，小米、华为、苹果等生态各自为政，用户若购买了不同品牌的智能设备，需要安装多个APP分别控制，无法实现统一管理。第三，市面上大多数智能音箱不具备触屏交互能力或屏幕极小，信息展示能力有限，人机交互手段单一。')
body('针对以上痛点，本作品设计了一款端侧处理能力强、具备大屏交互、支持分布式传感器网络、协议标准化的智能语音AI管家。用户通过语音即可控制家居设备、查询环境数据，同时7寸触摸屏提供直观的可视化操作界面。即使互联网暂时中断，本地设备控制和传感器数据查询仍可正常运行。')

add_h2('1.2  嵌入式系统开发的可移植性困境')
body('当前嵌入式物联网开发普遍基于FreeRTOS等实时操作系统，开发者直接调用厂商SDK提供的原生API进行任务创建、队列通信和事件同步。以ESP-IDF框架为例，大量项目代码中散布着xTaskCreate、xQueueSend、xEventGroupWaitBits等FreeRTOS专属函数调用。一旦需求变更需要将系统移植到NuttX、Zephyr等其他RTOS平台，上层应用代码必须逐文件修改API调用，工作量大且容易引入错误。这种厂商锁定的架构模式严重制约了物联网软件工程的可维护性和复用价值。')
body('本作品针对这一问题，设计并实现了一套POSIX标准化的线程模型抽象层，将全部业务代码中的RTOS原生API封装为统一的POSIX接口。上层68个源文件不再直接依赖任何RTOS专属符号，移植到新平台只需重新实现底层4个头文件，极大降低了代码迁移成本。')

add_h2('1.3  物联网设备协议碎片化问题')
body('物联网领域的设备通信协议长期处于碎片化状态。MQTT、CoAP、HTTP、Zigbee、BLE、Z-Wave等多种协议并存，不同厂商采用不同的数据模型和接口规范，设备之间的互操作性极差。Matter协议由CSA连接标准联盟联合苹果、谷歌、亚马逊等产业巨头共同推出，旨在通过统一的应用层标准实现跨品牌、跨生态的智能家居设备互联互通。然而，目前Matter认证芯片和完整协议栈成本仍然较高，对于教育和竞赛类项目而言使用门槛较高。')
body('本作品参照Matter标准的设计理念，自研了一套轻量级的"类Matter协议"设备模型框架。在不引入完整Matter协议栈的前提下，采用与Matter兼容的Endpoint→Cluster→Attribute三层架构来管理设备，通过标准化的JSON描述符暴露设备能力，为未来平滑升级到完整Matter认证方案预留通道。这一做法既降低了当前系统的复杂度，又保证了架构的前瞻性。')

# ── 第二章 特色与创新 ──
doc.add_page_break()
add_h1('第二章  特色与创新')
body('本作品的创新点集中在系统架构层面，而非单纯的功能堆叠。以下从软件架构、系统可靠性和协议标准化三个维度阐述四项核心创新。')

add_h2('2.1  POSIX标准化线程模型')
body('在传统嵌入式开发中，开发者直接使用芯片厂商SDK提供的RTOS原生API，导致应用代码与底层操作系统强耦合。例如使用ESP32的xTaskCreate创建任务，如果更换为STM32平台，则需要将所有任务创建代码改为CMSIS-RTOS的osThreadNew。这种厂商锁定效应使得代码几乎不具备跨平台移植能力。')
body('本作品设计并实现了posix_compat抽象层，将thread、queue、event_group、timer四类最常用的RTOS原语封装为标准POSIX风格接口。任务创建统一为posix::task_create({"task_name", stack_size, priority, core}, function)形式，事件等待统一为event_group->wait_bits(mask, clear, wait_all, timeout_ms)语义。整个项目68个业务源文件全部通过该抽象层与底层RTOS交互，不包含任何FreeRTOS专属API的直接调用。当需要移植到NuttX、Zephyr或其他RTOS时，仅需重新实现4个头文件中声明的接口，上层68个业务文件无需任何修改。')
body('此外，该抽象层还带来了工程管理的附加收益。统一的API风格降低了新成员的上手难度；集中的接口定义使得调试时可以在一处设置断点监控所有任务调度行为；所有任务的栈大小和优先级集中在创建参数中声明，便于进行系统资源规划和优化。')

add_h2('2.2  内存隔离保护机制')
body('FreeRTOS等实时操作系统采用扁平内存模型，所有任务共享同一个地址空间。在这种模型下，任何一个模块的野指针越界写入都可能破坏另一个完全不相关模块的数据结构，导致整机死机或行为异常。这种故障的特点是随机性强、难以复现、查错成本极高。')
body('本作品自研了一套轻量级内存保护框架mem_protect，包含三道防线。第一道是5域分区内存池（AUDIO、DISPLAY、CAMERA、NETWORK、SYSTEM），每个模块必须在自己的域中申请内存，跨域释放被拒绝并产生告警日志。第二道是魔数哨兵检测，每块分配的内存首尾各附加固定魔数值（0xDEADBEEF和0xCAFEBABE），释放和定期巡检时校验魔数完整性，一旦发现被覆盖立即输出精确的越界地址和破坏模块信息。第三道是RISC-V PMP硬件写保护，利用ESP32-P4芯片内建的内存保护单元，将代码段和只读数据段配置为不可写，任何野指针试图向代码段写入数据时触发CPU硬件异常，精确停在越界指令处。')
body('该机制在工程上具有显著价值。调试阶段可以快速定位野指针Bug的具体位置，避免耗费数小时排查随机死机问题；生产环境中可以作为安全防护层，防止单模块故障扩散为全系统崩溃，提升产品可靠性。')

add_h2('2.3  类Matter协议设备模型')
body('Matter协议定义了Endpoint（端点）→Cluster（功能簇）→Attribute（属性）的三层设备描述架构。这一标准模型使得不同厂商的设备可以在统一的语义下互操作。然而，完整的Matter协议栈需要Matter认证芯片、Thread/BLE底层通信和复杂的Commissioning流程，对于采用WiFi和MQTT通信的ESP32项目而言过于沉重。')
body('本作品遵循"架构对齐、实现简化"的原则，自研了matter_device模块，实现了完整的Endpoint→Cluster→Attribute三层设备管理框架，支持属性的读写回调绑定、命令注册和执行、以及设备树的JSON序列化输出。系统启动时调用smarthome_matter_init()一次性注册三个Endpoint：本地环境传感器（含温湿度、光照三个Cluster）、本地执行器（含风扇、灯光两个Cluster）、Room2远程传感器（数据源来自C3节点）。MCP工具接口从原来需要为每个设备手写专用的get/set/on/off接口，变为3个通用工具统一适配所有设备。新增一个设备只需在初始化时注册相应的Endpoint和Cluster，无需编写任何控制接口代码。Web API新增的GET /api/matter/descriptor端点返回完整设备树JSON，Web前端可动态解析此描述符自动生成控制UI。更为重要的是，该框架采用与Matter标准兼容的数据模型格式，当未来需要接入真实的Matter认证网络时，只需替换底层通信层和绑定Matter SDK的回调，上层设备模型几乎无需修改。')

add_h2('2.4  mDNS零配置分布式感知网络')
body('本作品采用ESP32-C3作为远程传感器节点，独立部署在另一个房间，通过WiFi向P4上报温湿度和光照数据。在传统方案中，C3必须硬编码P4的IP地址才能发起HTTP POST请求，然而P4作为WiFi客户端，其IP由路由器DHCP动态分配，每隔几天就会变化。每次IP变化后C3上报中断，用户必须手动修改C3固件代码并重新编译烧录，运维体验极差。')
body('本作品引入mDNS组播DNS服务发现机制解决这一问题。P4在WiFi连接成功后调用mdns_init()并注册主机名xiaozhi.local；C3启动后同样初始化mDNS，通过标准getaddrinfo("xiaozhi.local")函数解析出P4当前的IP地址。整个过程不依赖任何中心化DNS服务器或固定IP配置，完全由局域网组播自动完成。C3每10分钟自动刷新一次解析结果以适应P4可能发生的IP变更，首次烧录后永久免维护，真正实现了分布式感知节点的即插即用。在实施过程中解决了ESP-IDF v5.5框架下mDNS组件从框架内置迁移至Component Manager的依赖配置问题，以及ESP32-C3 RISC-V小端架构上LWIP网络字节序转换的关键技术点。')

# ── 第三章 功能设计 ──
doc.add_page_break()
add_h1('第三章  功能设计')
body('本章详细描述作品根据需求分析所规划设计的各项功能及其作用。')

add_h2('3.1  智能语音助手功能')
add_h3('3.1.1  语音唤醒')
body('系统搭载ESP-ADF Audio Front-End音频前端处理管线，集成噪声抑制、人声活动检测和回声消除功能。用户说出唤醒词"你好小智"，系统即自动识别并进入对话模式，唤醒词识别率在安静环境下达到95%以上。唤醒后系统播放提示音反馈，并在屏幕上显示聆听状态。')
add_h3('3.1.2  实时语音对话')
body('进入对话模式后，麦克风采集的16kHz PCM音频通过Opus编码器压缩为60ms帧长的数据包，经WebSocket安全通道推送至云端ASR引擎进行语音识别。识别结果送入LLM大语言模型进行语义理解和推理生成回复，TTS引擎将回复文本合成为自然语音下发至设备端解码播放。整个过程端到端延迟在2秒以内，对话气泡界面同步显示用户输入和AI回复文本内容，支持多轮对话上下文记忆。')
add_h3('3.1.3  多模式切换')
body('系统支持自动停止、手动停止和实时对话三种交互模式。自动停止模式下VAD检测到用户停止说话后自动结束聆听；手动停止模式由用户通过触摸屏按键控制对话起止；实时对话模式配合AEC回声消除实现全双工通话体验。')

add_h2('3.2  智能家居控制功能')
add_h3('3.2.1  环境传感数据采集与展示')
body('本地DHT22温湿度传感器和BH1750光照传感器每2秒采集一次数据，LVGL图形界面以仪表盘形式实时显示温度（精确到0.1℃）、湿度（精确到0.1%RH）和光照强度（精确到0.1勒克斯）。远程ESP32-C3节点每5秒通过HTTP POST将Room2房间的温湿度和光照数据上报至P4，数据融合后在同一界面显示。传感器数据同时通过Web API提供给内置的智能家居Web面板。')
add_h3('3.2.2  多方式设备控制')
body('风扇通过GPIO36/37输出PWM信号实现无级调速，灯光通过GPIO12开关量控制。用户可通过三种方式进行操作：说出语音指令直接控制；在7寸触摸屏的Smart Home面板上点击控制卡片；在手机上连接设备SoftAP热点后通过Web网页远程控制。系统同时支持自动模式，当温度超过预设阈值时自动开启风扇，低于阈值时自动关闭，实现免干预的智能环境调节。')
add_h3('3.2.3  人脸检测与自动解锁')
body('MIPI CSI摄像头通过PPA硬件缩放模块将800×800实时视频流缩放至600×600后在屏幕上渲染。P4Camera模块持续进行人体活动检测，当检测到人脸时自动解锁锁屏界面，5秒无检测后人脸框自动消失。开机设有3秒保护期，防止误触发。')
add_h3('3.2.4  Web智能家居面板')
body('设备内建完整的单页Web应用，运行于http://192.168.4.1。包含五个Tab页面：首页仪表盘展示温湿度和光照的实时数据卡片、风扇和灯光的快捷控制面板，支持Room1和Room2一键切换查看两个房间的传感器数据；设备页面管理所有智能设备；AI对话页面提供与小智的文字聊天功能；摄像头页面提供JPEG实时推流画面；设置页面预留设备管理和网络配置入口。整个Web页面为精心设计的暗色主题现代UI，无需任何外部依赖，所有HTML、CSS和JavaScript代码内嵌于固件中。')
add_h3('3.2.5  屏端WiFi设置功能')
body('为解决嵌入式设备WiFi凭据硬编码导致换网必须重新烧录固件的痛点，本作品在状态栏右上角增设了齿轮形状的WiFi设置入口。用户点击齿轮图标后，系统弹出半透明遮罩层和WiFi网络列表面板，后台自动执行WiFi全信道扫描。列表项展示每个WiFi网络的SSID名称（支持中文字符）、信号强度图标（强/中/弱三档分级）、加密类型图标（锁/解锁）和RSSI数值。每5秒自动刷新扫描结果，用户也可点击刷新按钮手动触发。点击目标网络后，开放网络直接选择连接，加密网络则弹出密码输入对话框，调用板载LVGL虚拟键盘输入密码。提交后系统将凭据保存至NVS非易失存储并自动重启，开机即自动连接新更换的网络，彻底摆脱烧录依赖。')
add_h3('3.2.6  环境数据历史曲线')
body('作品内置了三个环境参数的实时历史曲线图表，分别展示温度（量程0至40摄氏度）、湿度（量程0至100%相对湿度）和光照强度（量程0至1500勒克斯）随时间变化的趋势。每个图表采用LVGL折线图控件，横轴记录最近120个采样点（约2分钟窗口），纵轴标注5个数值刻度，支持自动缩放。采样定时器每秒从DHT22和BH1750传感器读取一次数据，将新数值压入数据队列头部并淘汰最旧的数据点，调用lv_chart_refresh()刷新曲线显示。三个图表以LVGL标签页的形式独立排列，用户通过左右滑动或旋钮即可切换查看不同参数的历史变化趋势，直观分析室内环境的动态变化规律。')
add_h2('3.3  安全与系统管理功能')
add_h3('3.3.1  OTA远程固件升级')
body('设备每次启动后自动访问服务器进行版本检查，上传设备MAC地址、UUID和当前固件版本等信息。服务器返回最新固件版本号与下载地址，设备端进行语义版本号比对（支持三段式数字版本号及git describe格式），若存在更新则全量下载固件并通过ESP-IDF的OTA分区切换机制安全升级。升级过程在屏幕显示进度百分比和下载速率，升级成功后自动重启，支持强制升级模式。')
add_h3('3.3.2  设备激活与安全认证')
body('设备首次联网需完成激活流程，采用HMAC-SHA256挑战应答认证机制，利用ESP32芯片内置eFuse块的HMAC硬件密钥进行安全身份验证。激活码通过TTS语音逐位播报给用户，有效防止中间人攻击。')
add_h3('3.3.3  时间同步与断线重连')
body('设备通过服务器返回的时间戳和时区偏移量自动同步系统时钟。MQTT连接断开后采用指数退避策略自动重连，首次重试间隔5秒，逐次倍增至60秒封顶。当重连成功时计数器归零，确保在不稳定网络环境下也能快速恢复连接。')
add_h3('3.3.4  低功耗锁屏模式')
body('作品集成了低功耗锁屏保护功能。当设备处于空闲状态一定时间后，系统自动进入锁屏模式，在LVGL的最顶层显示纯黑半透明锁屏界面，仅展示当前时间和"点击唤醒"提示文字。锁屏状态下，LCD背光亮度自动调低至最低可见水平，ES8311音频Codec的输出通道自动禁用以节省功耗。用户点击屏幕任意位置即可退出锁屏恢复正常操作界面，恢复时背光亮度和Codec输出通道自动还原至锁屏前状态。摄像头的人脸检测功能与锁屏联动，当摄像头检测到有人靠近时自动解锁，无需手动点击屏幕，实现了智能化的节能与便捷交互平衡。')

# ── 第四章 系统实现 ──
doc.add_page_break()
add_h1('第四章  系统实现')
body('本章描述实现上述功能所采用的物联网技术架构，涵盖感知层、传输层、控制层、软件开发环境和云应用五个层面的技术方案。')

add_h2('4.1  感知层技术')
body('本作品部署了本地与远程双层感知网络，采集环境数据和用户输入。')
body('温湿度采集采用DHT22数字温湿度传感器，内置电容式湿度传感元件和NTC热敏电阻，通过单总线数字接口与MCU通信，测量范围温度-40至80℃、湿度0至100%RH，精度分别为±0.5℃和±2%RH。驱动层采用状态机方式处理传感器的启动信号和40位数据读取时序。P4本地和C3远程各部署一个DHT22。')
body('光照采集采用BH1750FVI数字光照传感器，内置16位模数转换器，通过I2C接口通信，测量范围1至65535勒克斯，分辨率最高0.5勒克斯。驱动层支持单次测量和连续测量两种模式。P4本地使用GPIO21/GPIO33，C3远程使用GPIO8/GPIO10。')
body('视频采集采用OV5647 MIPI CSI摄像头传感器，通过2-lane MIPI D-PHY接口连接ESP32-P4的CSI控制器，分辨率配置为800×800像素、RGB565色彩格式。XCLK主时钟由LEDC PWM输出20MHz信号至GPIO32。SCCB控制总线通过GPIO28/29与传感器通信。驱动层基于Linux V4L2标准框架，提供open/ioctl/mmap/munmap等标准文件操作接口。')
body('触摸交互采用GT911电容触摸面板控制器，通过I2C接口连接，支持5点同时触摸。旋转编码器采用EC11型号，S1和S2引脚用于采集正交脉冲判断旋转方向和步数，KEY引脚检测按下事件。')
body('音频采集通过ES8311音频Codec芯片实现，通过I2S接口与ESP32-P4连接，支持双通道（麦克风+参考信号）24kHz 16bit音频输入和单通道音频输出。Codec初始化为Slave模式，音频输入和输出通道可独立使能以实现按需功耗管理。')

add_h2('4.2  传输层技术')
body('本作品的通信架构具备四层结构：芯片间互联、局域网接入、服务发现和云端通信。')
body('芯片间互联层。ESP32-P4与ESP32-C6之间通过SDIO 2.0接口连接，时钟频率40MHz、4-bit数据总线，峰值吞吐量约160Mbps。通信协议栈采用乐鑫ESP-Hosted框架，C6端运行完整的WiFi协议栈和TCP/IP栈，P4端通过transport层提供的标准网络接口透明访问网络。')
body('局域网接入层。C6芯片提供2.4GHz WiFi 802.11 b/g/n接入能力，同时运行STA客户端模式和SoftAP热点模式。STA模式连接外部路由器或手机热点获取互联网访问；SoftAP模式以Xiaozhi-SmartHome为SSID创建开放热点，IP地址固定为192.168.4.1，最多支持4个客户端同时连接。底层网络栈为ESP-NETIF加LWIP，提供标准BSD Socket接口。')
body('服务发现层。基于mDNS组播DNS协议实现，P4端在WiFi连接成功后初始化mDNS服务，注册xiaozhi.local主机名。C3端通过标准getaddrinfo() API解析P4的IPv4地址。关键实现细节：ESP-IDF v5.5已将mDNS从框架内置组件迁移至IDF Component Manager，需在idf_component.yml中添加espressif/mdns依赖；ESP32-C3为RISC-V小端架构，getaddrinfo返回的sin_addr.s_addr为网络字节序，必须使用ntohl()转换后才能正常使用。C3定时器每10分钟自动刷新解析结果。')
body('云端通信层。MQTT协议用于信令交互和设备管理，连接mqtt.xiaozhi.me服务器，TLS加密传输，主要包括设备注册与认证、心跳保活（60秒间隔）、JSON格式文本消息收发等功能，断线后采用指数退避自动重连。WebSocket协议用于实时语音流传输，连接wss://api.tenclass.net/xiaozhi/v1/，支持二进制帧（Opus数据包）和文本帧（JSON控制消息）。设备模型标准接口通过HTTP协议暴露，GET /api/matter/descriptor返回标准化的设备树JSON描述符，C3节点的传感器数据上报通过POST /api/room2/sensors接口。')

add_h2('4.3  控制层技术')
body('风扇控制。采用双路GPIO配合软件PWM实现无级调速，PWM频率2kHz、分辨率8位（0至255级）。当速度值为0时两路均输出低电平实现完全关断。自动模式下，系统根据DHT22实时温度与预设阈值比较决定开关动作，设置2℃滞后区间避免频繁切换。')
body('灯光控制。采用GPIO12数字输出，高电平有效驱动LED。软件层面通过smart_home_state命名空间维护风扇和灯光的逻辑状态，与硬件GPIO操作解耦。当自动模式激活时，手动控制请求被拒绝并提示用户先切换至手动模式。')
body('屏幕背光调节。ESP32-P4内建LDO控制器通道3提供500至2700mV可调电压输出。系统根据BH1750采集的环境光照值，使用分段线性映射算法实时调节屏幕亮度，实现类似手机自动亮度的效果，同时节约功耗。')
body('WiFi网络管理。通过自研的wifi_settings模块实现屏端WiFi网络扫描与切换功能。后台扫描任务以阻塞式API运行在独立FreeRTOS任务中，扫描结果通过LVGL定时器轮询机制安全更新UI。网络列表以LVGL flex布局渲染每行52像素高度的可点击卡片，包含信号强度图标、SSID名（使用项目中文字体避免方框问题）、锁型加密图标和RSSI数值。密码输入对话框采用LVGL内置虚拟键盘叠加文本框方案，卡片置于上半屏避免与底部键盘重叠。选择网络并输入密码后，凭据通过SsidManager保存至NVS并调用esp_restart()安全重启，避免运行时WiFi栈重建导致的netif冲突崩溃。')
body('视频处理管线。摄像头800×800 RGB565原始帧通过PPA硬件SRM模块缩放至600×600，输出到LVGL Canvas。PPA配置为Blocking模式，每次处理一帧后等待完成。处理后对输出缓冲区执行缓存同步确保CPU读到PPA硬件写入的最新数据，3帧轮转缓冲机制在PPA写入、Web推流拷贝和LVGL渲染读取之间实现无锁流水线。')
body('历史数据曲线。通过LVGL折线图控件实现温湿度、光照三个参数的实时趋势显示。每个图表维护包含120个数据点的循环队列，每秒从传感器采集数据并更新图表。纵轴支持自动缩放，横轴随新数据到达自动滚动。三个图表以Tile标签页形式独立排列在三个连续页面中，用户通过滑动或旋钮切换查看，提供直观的环境变化趋势分析功能。')
body('音频处理管线。系统维护两条平行的AFE处理链：一条用于语音通信（AfeAudioProcessor，优先级8），包含噪声抑制、VAD和可选AEC；另一条用于唤醒词检测（AfeWakeWord，优先级3），加载ESP-SR提供的wakenet模型。两条链共享音频Codec的输入通道但独立运行，通过EnableWakeWordDetection/EnableVoiceProcessing函数在功能间切换。上行管线：PCM→AFE处理→Opus编码→发送队列→WebSocket推送。下行管线：MQTT接收→Opus解码→重采样→播放队列→I2S输出。')

add_h2('4.4  软件开发环境')
body('操作系统与框架。基于FreeRTOS实时操作系统，使用乐鑫官方ESP-IDF v5.5.1物联网开发框架。项目构建采用CMake加Ninja系统，依赖管理使用IDF Component Manager自动拉取并锁定第三方组件版本。编译工具链为RISC-V 32-bit ESP-ELF GCC 14.2.0，目标芯片esp32p4。')
body('编程语言与图形库。应用层采用C++17标准编写，利用面向对象、RAII资源管理、lambda闭包和模板等现代C++特性提升代码质量。底层驱动和传感器库使用C语言以保持与硬件寄存器操作的高效对接。图形用户界面基于LVGL v8.4.0嵌入式图形库构建，利用其轻量级、高性能和丰富的控件库实现流畅的触摸交互体验。')
body('音视频编解码组件。音频编码采用Opus编码器，配置为16kHz采样率、单声道、60ms帧长、复杂度0（最快模式），算法延迟仅26.5ms。音频重采样使用OpusResampler组件，将Codec输出的24kHz音频转换为16kHz供AFE处理。视频缩放依靠PPA硬件加速器完成，JPEG编码使用ESP32-P4内置的JPEG硬件编码器，Web推流配置为质量60%以平衡清晰度与低延迟。')
body('自研软件模块。作品包含三个原创软件模块。posix_compat模块（5个头文件）封装thread、queue、event_group和timer四类POSIX标准接口，当前基于FreeRTOS实现，接口设计兼容NuttX和Zephyr的POSIX子集。mem_protect模块（4个文件）实现分区内存池管理、魔数哨兵边界检测和PMP硬件保护配置。matter_device模块（6个文件）实现Endpoint、Cluster、Attribute三层设备模型，支持属性读写、命令调用和JSON描述符序列化，已注册3个Endpoint、7个Cluster和8个Attribute。')

add_h2('4.5  云应用')
body('固件管理服务。服务器端运行于https://api.tenclass.net/xiaozhi/ota/，提供RESTful API接受设备端的版本查询请求。请求采用HTTP POST方法，请求体为JSON格式，包含设备类型、MAC地址、客户端ID等身份信息。服务器返回的JSON响应包含firmware对象（version和url字段）、mqtt对象（连接参数）、websocket对象（连接参数）、server_time对象（时间戳和时区偏移）以及可选的activation对象（激活码和挑战值）。设备端使用cJSON库解析响应，分别提取各配置项写入NVS持久化存储。')
body('消息中转服务。MQTT Broker运行于mqtt.xiaozhi.me，采用TLS加密和用户名密码认证。P4设备使用服务器下发的唯一client_id连接，订阅设备专属主题以接收服务端推送消息。MQTT服务端通过hello消息下发session_id和UDP服务器信息，设备据此建立语音传输通道。WebSocket服务器运行于wss://api.tenclass.net/xiaozhi/v1/，设备使用Token认证建立连接，二进制帧承载Opus编码的语音数据。')
body('AI语音对话服务。云端提供完整的ASR→LLM→TTS管线。ASR引擎将设备上传的Opus音频流实时转写为文本后通过MQTT下发至设备端显示；LLM大语言模型基于上下文进行语义理解和推理，生成的回复文本通过MQTT下发，Web前端和屏幕同步更新对话气泡；TTS引擎将回复文本合成为Opus编码的音频流，通过MQTT或WebSocket下发至设备端解码播放。')

# ── 第五章 其他内容 ──
doc.add_page_break()
add_h1('第五章  其他内容')
body('本章描述前文未涵盖的重要内容，包括异构硬件架构设计和成本分析。')

add_h2('5.1  异构分布式硬件架构')
body('本作品采用"一主控+一通信协处理+一远程传感节点"的3芯片异构架构，充分挖掘乐鑫ESP32系列不同芯片的差异化技术优势。')
body('ESP32-P4作为主控芯片，搭载360MHz双核RISC-V处理器、32MB片外PSRAM和16MB Flash。其核心优势在于强大的多媒体处理能力：内置MIPI DSI显示控制器直接驱动7寸1024×600液晶屏；内置MIPI CSI摄像头控制器配合PPA硬件加速模块和JPEG硬件编码器实现高效的视频采集与处理；I2S音频接口配合ES8311 Codec提供高质量的音频输入输出；SDIO Host控制器通过ESP-Hosted框架与C6芯片通信。P4承担系统中全部重负载任务：LVGL图形渲染、AFE音频处理、Opus编解码、PPA视频缩放、人脸检测、Web服务器和Matter设备管理等。')
body('ESP32-C6作为WiFi通信协处理器，通过SDIO 2.0接口与P4连接。C6内置2.4GHz WiFi 6和Bluetooth 5.3基带，运行完整的WiFi协议栈和TCP/IP栈。采用ESP-Hosted框架使得P4视角下网络栈完全透明——P4只需调用标准的Http/Mqtt/Udp等网络接口，底层适配由transport层自动完成。这种"主控+通信协处理"的架构分离了应用处理与通信处理，各自可独立优化。')
body('ESP32-C3作为远程传感器节点，搭载160MHz单核RISC-V处理器和4MB Flash。C3的低功耗特性（运行模式仅87mA）使其非常适合作为电池供电的远程传感器节点。C3通过I2C接口采集DHT22和BH1750数据，通过WiFi STA模式连接路由器，以mDNS服务发现P4主机后通过HTTP POST每5秒上报JSON格式的传感器数据。C3固件由独立的ESP-IDF项目编译，与P4主项目在同一个Git仓库中管理，便于协同版本控制。')

add_h2('5.2  工业设计与成本分析')
body('本作品基于慧勤智远WKS-P4-CB开发板搭建。核心BOM成本估算如下：ESP32-P4模组约60元，ESP32-C6模组约15元，ESP32-C3模组约8元，7寸MIPI DSI显示屏约80元，OV5647摄像头模组约25元，DHT22传感器约3元两个，BH1750传感器约3元两个，ES8311音频Codec模块约8元，GT911触摸面板约15元，EC11编码器约2元，电源管理、PCB及其他被动元件约20元。整套系统物料成本约239元，若批量生产（1000套以上规模）成本可控制在150元以内。')
body('从工业设计角度，当前原型阶段将所有模块通过FPC排线和杜邦线连接，体积约18cm×12cm×5cm，适合竞赛演示和开发调试。若进行产品化量产，可通过定制一体化PCB将P4、C6、Codec、电源管理等核心元件集成在单板上（约8cm×6cm），屏幕和摄像头分别通过FPC排线连接，外壳采用注塑工艺，整体可控制在约12cm×10cm×8cm的尺寸内。显示面板建议采用全贴合工艺减少反射，麦克风阵列采用MEMS数字麦克风替代ECM模拟麦克风以提升远场语音识别性能。')

add_h2('5.3  项目开源地址')
body('本作品的全部源代码采用Git进行版本管理，托管于GitHub开源仓库。项目地址为 https://github.com/hhchhc1/hanma2.0 。仓库中包含ESP32-P4主控固件（main目录）、ESP32-C3远程传感器节点固件（c3_sensor_node目录）、设计文档生成脚本（build_doc.py）以及完整的设计文档（.docx格式）。所有代码遵循清晰的项目结构组织，主要模块包括显示层（display）、音频服务（audio）、传感器驱动（sensors）、通信协议（protocols）、POSIX标准化抽象层（posix_compat）、内存隔离保护（mem_protect）和类Matter设备模型（matter_device）等，便于评审专家查阅和复现。')

# ── 参考文献 ──
doc.add_page_break()
add_h1('参考文献')

refs = [
    '[1] CSA Connectivity Standards Alliance. Matter 1.4 Specification[EB/OL]. https://csa-iot.org/developer-resource/specifications-download-request/, 2025.',
    '[2] ESP-IDF Programming Guide v5.5.1[EB/OL]. Espressif Systems, https://docs.espressif.com/projects/esp-idf/en/v5.5.1/, 2025.',
    '[3] LVGL v8.4.0 Documentation[EB/OL]. LVGL Kft., https://docs.lvgl.io/8.4/, 2024.',
    '[4] S. Cheshire, M. Krochmal. RFC 6762: Multicast DNS[S]. IETF, 2013.',
    '[5] J. M. Valin, K. Vos, T. Terriberry. RFC 6716: Definition of the Opus Audio Codec[S]. IETF, 2012.',
    '[6] 王智, 潘强, 邢涛. 面向物联网的实体实时搜索服务综述[J]. 中国科学院上海微系统与信息技术研究所, 2009.',
    '[7] Andrew S. Tanenbaum, Herbert Bos. Modern Operating Systems (5th Edition)[M]. Pearson, 2022.',
    '[8] Espressif Systems. ESP32-P4 Technical Reference Manual[EB/OL]. https://www.espressif.com/sites/default/files/documentation/esp32-p4_technical_reference_manual_en.pdf, 2024.',
    '[9] Espressif Systems. ESP-Hosted Solution[EB/OL]. https://github.com/espressif/esp-hosted, 2025.',
    '[10] Andrew Banks, Rahul Mahajan. OASIS Standard: MQTT Version 5.0[S]. OASIS, 2019.',
    '[11] RISC-V International. The RISC-V Instruction Set Manual Volume II: Privileged Architecture[EB/OL]. https://riscv.org/technical/specifications/, 2024.',
]
for ref in refs:
    add_ref(ref)

# ═══════════════════════════════════════════
# Save
# ═══════════════════════════════════════════
doc.save(OUTPUT)
print(f'Done: {os.path.getsize(OUTPUT)} bytes')
