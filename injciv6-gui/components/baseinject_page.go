package components

import (
	"fmt"
	"os"
	"os/exec"
	"strings"

	"injciv6-gui/service"
	"injciv6-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type BaseInjectPageCfg struct {
	GetConfigContent        func() string
	GetStartStopButtonReady func() bool
	OnGameStatus            func(status utils.Civ6Status)
	OnInjectStatus          func(injectStatus utils.InjectStatus)
	OnInfo                  func(msg string)
	OnError                 func(err error)
}

type BaseInjectPage struct {
	*walk.Composite
	baseInjectPageCfg       *BaseInjectPageCfg
	gameStatusLabel         *walk.Label
	injectStatusLabel       *walk.Label
	closeAfterStartCheckBox *walk.CheckBox
	startStopButton         *walk.PushButton
}

func NewBaseInjectPage(parent walk.Container, cfg *BaseInjectPageCfg) (*BaseInjectPage, error) {
	p := &BaseInjectPage{
		baseInjectPageCfg: cfg,
	}

	if err := (Composite{
		Layout: HBox{MarginsZero: true},
		Children: []Widget{
			Composite{
				Layout: VBox{MarginsZero: true},
				Children: []Widget{
					Composite{
						Layout: HBox{MarginsZero: true},
						Children: []Widget{
							Label{
								Text:          "游戏状态: ",
								TextAlignment: AlignNear,
							},
							Label{
								Font:          Font{Bold: true},
								AssignTo:      &p.gameStatusLabel,
								Text:          "未知",
								TextColor:     ColorGray,
								Background:    SolidColorBrush{Color: ColorBackground},
								TextAlignment: AlignNear,
							},
						},
						Alignment: AlignHNearVCenter,
					},
					Composite{
						Layout: HBox{MarginsZero: true},
						Children: []Widget{
							Label{
								Text:          "注入状态: ",
								TextAlignment: AlignNear,
							},
							Label{
								Font:          Font{Bold: true},
								AssignTo:      &p.injectStatusLabel,
								Text:          "未注入",
								TextColor:     ColorRed,
								Background:    SolidColorBrush{Color: ColorBackground},
								TextAlignment: AlignNear,
							},
						},
						Alignment: AlignHNearVCenter,
					},
					CheckBox{
						Text:             "注入后关闭此程序",
						Checked:          service.GetWithDef("CloseAfterStart", true),
						OnCheckedChanged: p.OnCloseAfterStartChanged,
						AssignTo:         &p.closeAfterStartCheckBox,
						Alignment:        AlignHNearVCenter,
					},
				},
			},
			HSpacer{},
			Composite{
				Layout: VBox{},
				Children: []Widget{
					PushButton{
						Persistent: true,
						MinSize:    Size{Width: 230, Height: 80},
						Enabled:    false,
						Text:       "开始注入",
						AssignTo:   &p.startStopButton,
						OnClicked:  p.OnServerStartStopButtonClicked,
					},
				},
			},
		},
	}).Create(NewBuilder(parent)); err != nil {
		return nil, err
	}

	service.Game.Listener().Register(service.NewFuncListener(p.OnGameStatusChanged))
	service.Inject.Listener().Register(service.NewFuncListener(p.OnInjectStatusChanged))

	service.Game.Listener().Register(service.NewFuncListener(cfg.OnGameStatus))
	service.Inject.Listener().Register(service.NewFuncListener(cfg.OnInjectStatus))

	return p, nil
}

func (p *BaseInjectPage) LogInfo(msg string) {
	p.baseInjectPageCfg.OnInfo(msg)
}

func (p *BaseInjectPage) LogError(err error) {
	p.baseInjectPageCfg.OnError(err)
}

func (p *BaseInjectPage) OnCloseAfterStartChanged() {
	service.Set("CloseAfterStart", p.closeAfterStartCheckBox.Checked())
}

func (p *BaseInjectPage) OnGameStatusChanged(status utils.Civ6Status) {
	p.gameStatusLabel.SetSuspended(true)

	setGamePathTip := func() {
		path, err := utils.GetCiv6Path()
		if err != nil {
			err = fmt.Errorf("获取游戏路径失败: %v", err)
			p.gameStatusLabel.SetToolTipText(err.Error())
			go RerunAsAdmin(err.Error())
		}
		p.gameStatusLabel.SetToolTipText(path)
	}

	switch status {
	case utils.Civ6StatusRunningDX11:
		p.gameStatusLabel.SetText("运行中 (DX11)")
		p.gameStatusLabel.SetTextColor(ColorGreen)
		setGamePathTip()

	case utils.Civ6StatusRunningDX12:
		p.gameStatusLabel.SetText("运行中 (DX12)")
		p.gameStatusLabel.SetTextColor(ColorGreen)
		setGamePathTip()

	default:
		p.gameStatusLabel.SetText("未运行")
		p.gameStatusLabel.SetTextColor(ColorRed)
		p.gameStatusLabel.SetToolTipText("请先运行游戏")
	}
	p.gameStatusLabel.SetSuspended(false)

	go p.updateClientStartStopButton()
}

func (p *BaseInjectPage) OnInjectStatusChanged(injectStatus utils.InjectStatus) {
	switch injectStatus {
	case utils.InjectStatusInjected:
		p.injectStatusLabel.SetText("已注入")
		p.injectStatusLabel.SetTextColor(ColorGreen)
	case utils.InjectStatusNotInjected:
		p.injectStatusLabel.SetText("未注入")
		p.injectStatusLabel.SetTextColor(ColorRed)
	default:
		p.injectStatusLabel.SetText("未知")
		p.injectStatusLabel.SetTextColor(ColorGray)
	}
	go p.updateClientStartStopButton()
}

func (p *BaseInjectPage) updateClientStartStopButton() {
	enabled := false
	defer func() {
		p.startStopButton.SetEnabled(enabled)
	}()

	if !p.baseInjectPageCfg.GetStartStopButtonReady() {
		return
	}

	gameStatus := service.Game.Status()
	switch gameStatus {
	case utils.Civ6StatusRunningDX11, utils.Civ6StatusRunningDX12:
	default:
		return
	}

	injectStatus := service.Inject.IsInjected()
	switch injectStatus {
	case utils.InjectStatusRunningIPv6:
		p.startStopButton.SetText("请先返回至游戏主菜单")
		return
	case utils.InjectStatusInjected:
		p.startStopButton.SetText("移除注入")
	case utils.InjectStatusNotInjected:
		p.startStopButton.SetText("开始注入")
	default:
		p.startStopButton.SetText("未知")
	}
	enabled = true
}

func (p *BaseInjectPage) OnServerStartStopButtonClicked() {
	injectStatus := service.Inject.IsInjected()
	switch injectStatus {
	case utils.InjectStatusInjected, utils.InjectStatusRunningIPv6:
		p.StopInject()
	default:
		p.StartInject()
	}
}

func (p *BaseInjectPage) StartInject() {
	content := p.baseInjectPageCfg.GetConfigContent()
	if err := utils.WriteConfig(content); err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("写入配置文件失败: %v", errStr))
		return
	}
	path, ok := utils.GetInjectorPath()
	if !ok {
		p.LogError(fmt.Errorf("注入工具未找到"))
		return
	}
	cmd := exec.Command(path, "-s")
	err := cmd.Start()
	if err != nil {
		p.LogError(fmt.Errorf("启动注入工具失败: %v", err))
		return
	}
	if p.closeAfterStartCheckBox.Checked() {
		os.Exit(0)
	}
	p.LogInfo("启动注入工具成功")
}

func (p *BaseInjectPage) StopInject() {
	path, ok := utils.GetInjectRemoverPath()
	if !ok {
		p.LogError(fmt.Errorf("注入移除工具未找到"))
		return
	}
	cmd := exec.Command(path)
	err := cmd.Start()
	if err != nil {
		p.LogError(fmt.Errorf("启动注入移除工具失败: %v", err))
		return
	}
	p.LogInfo("启动注入移除工具成功")
}
