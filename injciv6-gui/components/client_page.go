package components

import (
	"fmt"
	"injciv6-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type ClientPage struct {
	*walk.Composite
	*BaseInjectPage
	serverAddrEdit     *walk.LineEdit
	serverAddrValid    bool
	baseInjectPageComp *walk.Composite
	infoLabel          *walk.Label
}

func NewClientPage(parent walk.Container) (Page, error) {
	p := &ClientPage{}

	if err := (Composite{
		AssignTo: &p.Composite,
		Name:     "联机",
		Layout:   VBox{},
		Children: []Widget{
			Label{
				Font: Font{PointSize: 10},
				Text: "服务器 IPv4 地址：",
			},
			LineEdit{
				AssignTo:          &p.serverAddrEdit,
				OnTextChanged:     p.OnServerAddrChanged,
				OnEditingFinished: p.OnServerAddrFinished,
			},
			VSeparator{},
			Composite{
				AssignTo: &p.baseInjectPageComp,
				Layout:   HBox{MarginsZero: true, SpacingZero: true},
			},
			VSpacer{},
			VSeparator{},
			Label{
				Font:          Font{PointSize: 10},
				TextAlignment: AlignFar,
				AssignTo:      &p.infoLabel,
				Text:          "　",
				Background:    SolidColorBrush{Color: ColorBackground},
			},
		},
	}).Create(NewBuilder(parent)); err != nil {
		return nil, err
	}

	if err := walk.InitWrapperWindow(p); err != nil {
		return nil, err
	}

	cfg := &BaseInjectPageCfg{
		GetConfigContent:        p.GetConfigContent,
		GetStartStopButtonReady: p.GetStartStopButtonReady,
		OnGameStatus:            p.OnGameStatusChanged,
		OnInjectStatus:          p.OnInjectStatusChanged,
		OnInfo:                  p.LogInfo,
		OnError:                 p.LogError,
	}

	var err error
	if p.BaseInjectPage, err = NewBaseInjectPage(p.baseInjectPageComp, cfg); err != nil {
		return nil, err
	}

	return p, nil
}

func (p *ClientPage) LogInfo(msg string) {
	p.infoLabel.SetText(msg)
	p.infoLabel.SetTextColor(ColorBlack)
}

func (p *ClientPage) LogError(err error) {
	p.infoLabel.SetText(err.Error())
	p.infoLabel.SetTextColor(ColorRed)
	fmt.Println(err)
}

func (p *ClientPage) OnServerAddrFinished() {
	addr := p.serverAddrEdit.Text()
	if utils.IsValidDomainRegex(addr) {
		go func() {
			if !utils.IsValidDomain(addr) {
				p.serverAddrValid = false
				p.serverAddrEdit.SetTextColor(ColorRed)
				p.LogError(fmt.Errorf("%s域名解析失败", addr))
				p.updateClientStartStopButton()
			}
		}()
	}
}

func (p *ClientPage) OnServerAddrChanged() {
	addr := p.serverAddrEdit.Text()
	valid := false
	defer func() {
		var color walk.Color
		if valid {
			color = ColorBlack
		} else {
			color = ColorRed
		}

		p.serverAddrEdit.SetTextColor(color)
		p.serverAddrValid = valid

		go p.updateClientStartStopButton()
	}()

	if utils.IsValidDomainRegex(addr) || utils.IsValidIPv4(addr) {
		valid = true
	}
}

func (p *ClientPage) OnGameStatusChanged(status utils.Civ6Status) {
	switch status {
	case utils.Civ6StatusRunningDX11, utils.Civ6StatusRunningDX12:
		if p.serverAddrEdit.Text() == "" {
			addr, err := utils.ReadConfig()
			if err != nil {
				p.LogError(fmt.Errorf("读取配置文件失败: %v", err))
			} else {
				p.serverAddrEdit.SetText(addr)
			}
		}
	default:
	}
}

func (p *ClientPage) OnInjectStatusChanged(injectStatus utils.InjectStatus) {
}

func (p *ClientPage) GetStartStopButtonReady() bool {
	return p.serverAddrValid
}

func (p *ClientPage) GetConfigContent() string {
	return p.serverAddrEdit.Text()
}
