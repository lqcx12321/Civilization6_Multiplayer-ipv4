package main

import (
	"kskbl-gui/components"
	"kskbl-gui/service"
	"kskbl-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

var (
	amw         *components.AppMainWindow
	appIcon     *walk.Icon
	remoteIcon  *walk.Icon
	toolIcon    *walk.Icon
	aboutIcon   *walk.Icon
)

func init() {
	appIcon, _ = walk.NewIconFromResourceId(3)
	remoteIcon, _ = walk.NewIconFromResourceId(5)
	toolIcon, _ = walk.NewIconFromResourceId(6)
	aboutIcon, _ = walk.NewIconFromResourceId(7)
}

func main() {
	if utils.IsAdmin() {
		seDebug := utils.GrantSeDebugPrivilege()
		service.Set("SeDebugPrivilege", seDebug)
	}
	cfg := &components.MultiPageMainWindowConfig{
		Name:    "mainWindow",
		Icon:    appIcon,
		Font:    Font{PointSize: 11},
		Size:    Size{Width: 550, Height: 200},
		MinSize: Size{Width: 550, Height: 200},
		PageCfgs: []components.PageConfig{
			{
				Title:   "联机",
				Image:   remoteIcon,
				NewPage: components.NewClientPage,
			},
			{
				Title:   "工具",
				Image:   toolIcon,
				NewPage: components.NewToolsPage,
			},
			{
				Title:   "关于",
				Image:   aboutIcon,
				NewPage: components.NewAboutPage,
			},
		},
	}
	amw, err := components.NewAppMainWindow("kskbl-gui", cfg)
	if err != nil {
		panic(err)
	}
	service.Set("AppMainWindowHandle", amw)
	amw.RefreshTitle()
	amw.Run()
}
