# - generate surface mesh of CAD -
try: import tkinter as TK
except: import Tkinter as TK
import CPlot.Ttk as TTK
import Converter.PyTree as C
import CPlot.PyTree as CPlot
import CPlot.Tk as CTK
import Converter.Internal as Internal

# local widgets list
WIDGETS = {}; VARS = []
    
#==============================================================================
def meshCADEdges(event=None):
    if CTK.CADHOOK is None: return
    if CTK.t == []: return
    import OCC.PyTree as OCC
    hmax = CTK.varsFromWidget(VARS[0].get(), 1)[0]
    hausd = CTK.varsFromWidget(VARS[1].get(), 1)[0]
    CTK.saveTree()
    CTK.setCursor(2, WIDGETS['frame'])
    # remesh CAD and redisplay
    edges = Internal.getNodeFromName1(CTK.t, 'EDGES')
    if edges is not None: edges[2] = []
    faces = Internal.getNodeFromName1(CTK.t, 'FACES')
    if faces is not None: faces[2] = []
    OCC._meshAllEdges(CTK.CADHOOK, CTK.t, hmax=hmax, hausd=hausd)
    CTK.setCursor(0, WIDGETS['frame'])
    (CTK.Nb, CTK.Nz) = CPlot.updateCPlotNumbering(CTK.t)
    CTK.TKTREE.updateApp()
    CTK.display(CTK.t)
    CTK.TXT.insert('START', 'All CAD edges remeshed.\n')

#==============================================================================
def meshCADFaces(event=None):
    if CTK.CADHOOK is None: return
    if CTK.t == []: return 
    mtype = VARS[2].get()
    import OCC.PyTree as OCC
    hmax = CTK.varsFromWidget(VARS[0].get(), 1)[0]
    hausd = CTK.varsFromWidget(VARS[1].get(), 1)[0]
    CTK.saveTree()
    CTK.setCursor(2, WIDGETS['frame'])
    CTK.setCursor(2, WIDGETS['meshFaceButton'])
    faces = Internal.getNodeFromName1(CTK.t, 'FACES')
    if faces is not None: faces[2] = []
    if mtype == 'TRI':
        OCC._meshAllFacesTri(CTK.CADHOOK, CTK.t, hmax=hmax, hausd=hausd)
    elif mtype == 'STRUCT':
        OCC._meshAllFacesStruct(CTK.CADHOOK, CTK.t)
    
    CTK.setCursor(0, WIDGETS['frame'])
    CTK.setCursor(0, WIDGETS['meshFaceButton'])
    
    (CTK.Nb, CTK.Nz) = CPlot.updateCPlotNumbering(CTK.t)
    CTK.TKTREE.updateApp()
    CTK.display(CTK.t)
    CTK.TXT.insert('START', 'All CAD faces meshed.\n')

#==============================================================================
# Create app widgets
#==============================================================================
def createApp(win):
    
    # - Frame -
    Frame = TTK.LabelFrame(win, borderwidth=2, relief=CTK.FRAMESTYLE,
                           text='tkCADMesh  [ + ]  ', font=CTK.FRAMEFONT, takefocus=1)
    #BB = CTK.infoBulle(parent=Frame, text='Mesh CAD surface.\nCtrl+w to close applet.', temps=0, btype=1)
    Frame.bind('<Control-w>', hideApp)
    Frame.bind('<ButtonRelease-1>', displayFrameMenu)
    Frame.bind('<ButtonRelease-3>', displayFrameMenu)
    Frame.bind('<Enter>', lambda event : Frame.focus_set())
    Frame.columnconfigure(0, weight=1)
    Frame.columnconfigure(1, weight=1)
    WIDGETS['frame'] = Frame
    
    # - Frame menu -
    FrameMenu = TTK.Menu(Frame, tearoff=0)
    FrameMenu.add_command(label='Close', accelerator='Ctrl+w', command=hideApp)
    CTK.addPinMenu(FrameMenu, 'tkCADMesh')
    WIDGETS['frameMenu'] = FrameMenu

    CAD = Internal.getNodeFromName1(CTK.t, 'CAD')
    if CAD is not None:
        hmax = Internal.getNodeFromName1(CAD, 'hsize')
        hmax = Internal.getValue(hmax)
        hausd = Internal.getNodeFromName1(CAD, 'hausd')
        hausd = Internal.getValue(hausd)
    else: hmax = 1.; hausd = 0.1

    #- VARS -
    # -0- hmax -
    V = TK.StringVar(win); V.set('%g'%hmax); VARS.append(V)
    # -1- hausd -
    V = TK.StringVar(win); V.set('%g'%hausd); VARS.append(V)
    # -2- TRI/STRUCT -
    V = TK.StringVar(win); V.set('TRI'); VARS.append(V)

    # Generate CAD surface mesh    
    B = TTK.Entry(Frame, textvariable=VARS[0], background='White', width=10)
    B.grid(row=0, column=0, sticky=TK.EW)
    BB = CTK.infoBulle(parent=B, text='H step of surface mesh.')
    B.bind('<Return>', meshCADEdges)

    B = TTK.Entry(Frame, textvariable=VARS[1], background='White', width=10)
    B.grid(row=0, column=1, sticky=TK.EW)
    BB = CTK.infoBulle(parent=B, text='Deviation of surface mesh.')
    B.bind('<Return>', meshCADEdges)

    B = TTK.Button(Frame, text="Mesh all CAD edges", command=meshCADEdges)
    B.grid(row=1, column=0, columnspan=1, sticky=TK.EW)
    BB = CTK.infoBulle(parent=B, text='Mesh all CAD egdes with given h and deviation.')

    B = TTK.Button(Frame, text="Mesh all CAD faces", command=meshCADFaces)
    B.grid(row=2, column=0, columnspan=1, sticky=TK.EW)
    BB = CTK.infoBulle(parent=B, text='Mesh the CAD faces from edges.')
    WIDGETS['meshFaceButton'] = Frame
    
    B = TTK.OptionMenu(Frame, VARS[2], 'TRI', 'STRUCT')
    B.grid(row=2, column=1, sticky=TK.EW)
    

#==============================================================================
# Called to display widgets
#==============================================================================
def showApp():
    try: CTK.WIDGETS['SurfNoteBook'].add(WIDGETS['frame'], text='tkCADMesh')
    except: pass
    CTK.WIDGETS['SurfNoteBook'].select(WIDGETS['frame'])
    
#==============================================================================
# Called to hide widgets
#==============================================================================
def hideApp(event=None):
    CTK.WIDGETS['SurfNoteBook'].hide(WIDGETS['frame'])

#==============================================================================
# Update widgets when global pyTree t changes
#==============================================================================
def updateApp(): return

#==============================================================================
def displayFrameMenu(event=None):
    WIDGETS['frameMenu'].tk_popup(event.x_root+50, event.y_root, 0)

#==============================================================================
if __name__ == "__main__":
    import sys
    if len(sys.argv) == 2:
        CTK.FILE = sys.argv[1]
        try:
            CTK.t = C.convertFile2PyTree(CTK.FILE)
            (CTK.Nb, CTK.Nz) = CPlot.updateCPlotNumbering(CTK.t)
            CTK.display(CTK.t)
        except: pass

    # Main window
    (win, menu, file, tools) = CTK.minimal('tkCADMesh '+C.__version__)

    createApp(win); showApp()

    # - Main loop -
    win.mainloop()