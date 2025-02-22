#***************************************************************************
#*                                                                         *
#*   Copyright (c) 2011                                                    *  
#*   Yorik van Havre <yorik@uncreated.net>                                 *  
#*                                                                         *
#*   This program is free software; you can redistribute it and/or modify  *
#*   it under the terms of the GNU Lesser General Public License (LGPL)    *
#*   as published by the Free Software Foundation; either version 2 of     *
#*   the License, or (at your option) any later version.                   *
#*   for detail see the LICENCE text file.                                 *
#*                                                                         *
#*   This program is distributed in the hope that it will be useful,       *
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#*   GNU Library General Public License for more details.                  *
#*                                                                         *
#*   You should have received a copy of the GNU Library General Public     *
#*   License along with this program; if not, write to the Free Software   *
#*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
#*   USA                                                                   *
#*                                                                         *
#***************************************************************************

__title__="FreeCAD Draft Snap tools"
__author__ = "Yorik van Havre"
__url__ = "http://free-cad.sourceforge.net"


import FreeCAD, FreeCADGui, math, Draft, DraftGui, DraftTrackers
from DraftGui import todo,getMainWindow
from draftlibs import fcvec
from FreeCAD import Vector
from pivy import coin
from PyQt4 import QtCore,QtGui

class Snapper:
    """The Snapper objects contains all the functionality used by draft
    and arch module to manage object snapping. It is responsible for
    finding snap points and displaying snap markers. Usually You
    only need to invoke it's snap() function, all the rest is taken
    care of.

    3 functions are useful for the scriptwriter: snap(), constrain()
    or getPoint() which is an all-in-one combo.

    The indivudual snapToXXX() functions return a snap definition in
    the form [real_point,marker_type,visual_point], and are not
    meant to be used directly, they are all called when necessary by
    the general snap() function.

    The Snapper lives inside FreeCADGui once the Draft module has been
    loaded.
    
    """

    def __init__(self):
        self.lastObj = [None,None]
        self.views = []
        self.maxEdges = 0
        self.radius = 0
        self.constraintAxis = None
        self.basepoint = None
        self.affinity = None
        self.cursorMode = None
        if Draft.getParam("maxSnap"):
            self.maxEdges = Draft.getParam("maxSnapEdges")

        # we still have no 3D view when the draft module initializes
        self.tracker = None
        self.extLine = None
        self.grid = None
        self.constrainLine = None
        self.trackLine = None
        self.lastSnappedObject = None
        self.active = True

        self.polarAngles = [90,45]
        
        # the snapmarker has "dot","circle" and "square" available styles
        self.mk = {'passive':'circle',
                   'extension':'circle',
                   'parallel':'circle',
                   'grid':'circle',
                   'endpoint':'dot',
                   'midpoint':'dot',
                   'perpendicular':'dot',
                   'angle':'dot',
                   'center':'dot',
                   'ortho':'dot',
                   'intersection':'dot'}
        self.cursors = {'passive':':/icons/Snap_Near.svg',
                        'extension':':/icons/Snap_Extension.svg',
                        'parallel':':/icons/Snap_Parallel.svg',
                        'grid':':/icons/Snap_Grid.svg',
                        'endpoint':':/icons/Snap_Endpoint.svg',
                        'midpoint':':/icons/Snap_Midpoint.svg',
                        'perpendicular':':/icons/Snap_Perpendicular.svg',
                        'angle':':/icons/Snap_Angle.svg',
                        'center':':/icons/Snap_Center.svg',
                        'ortho':':/icons/Snap_Ortho.svg',
                        'intersection':':/icons/Snap_Intersection.svg'}
        
    def snap(self,screenpos,lastpoint=None,active=True,constrain=False):
        """snap(screenpos,lastpoint=None,active=True,constrain=False): returns a snapped
        point from the given (x,y) screenpos (the position of the mouse cursor), active is to
        activate active point snapping or not (passive), lastpoint is an optional
        other point used to draw an imaginary segment and get additional snap locations. Constrain can
        be True to constrain the point against the closest working plane axis.
        Screenpos can be a list, a tuple or a coin.SbVec2s object."""

        global Part,fcgeo
        import Part
        from draftlibs import fcgeo

        if not hasattr(self,"toolbar"):
            self.makeSnapToolBar()
        mw = getMainWindow()
        bt = mw.findChild(QtGui.QToolBar,"Draft Snap")
        if not bt:
            mw.addToolBar(self.toolbar)
        else:
            if Draft.getParam("showSnapBar"):
                bt.show()

        def cstr(point):
            "constrains if needed"
            if constrain:
                return self.constrain(point,lastpoint)
            else:
                self.unconstrain()
                return point

        snaps = []
        
        # type conversion if needed
        if isinstance(screenpos,list):
            screenpos = tuple(screenpos)
        elif isinstance(screenpos,coin.SbVec2s):
            screenpos = tuple(screenpos.getValue())
        elif  not isinstance(screenpos,tuple):
            print "snap needs valid screen position (list, tuple or sbvec2s)"
            return None

        # setup trackers if needed
        if not self.tracker:
            self.tracker = DraftTrackers.snapTracker()
        if not self.extLine:
            self.extLine = DraftTrackers.lineTracker(dotted=True)
        if (not self.grid) and Draft.getParam("grid"):
            self.grid = DraftTrackers.gridTracker()

        # getting current snap Radius
        if not self.radius:
            self.radius =  self.getScreenDist(Draft.getParam("snapRange"),screenpos)

        # set the grid
        if self.grid and Draft.getParam("grid"):
            self.grid.set()
        
        # activate snap
        oldActive = False
        if Draft.getParam("alwaysSnap"):
            oldActive = active
            active = True
        if not self.active:
            active = False

        self.setCursor('passive')
        if self.tracker:
            self.tracker.off()
        if self.extLine:
            self.extLine.off()

        point = self.getApparentPoint(screenpos[0],screenpos[1])
            
        # check if we snapped to something
        info = FreeCADGui.ActiveDocument.ActiveView.getObjectInfo((screenpos[0],screenpos[1]))

        # checking if parallel to one of the edges of the last objects or to a polar direction

        if active:
            eline = None
            point,eline = self.snapToPolar(point,lastpoint)
            point,eline = self.snapToExtensions(point,lastpoint,constrain,eline)
        
        if not info:
            
            # nothing has been snapped, check fro grid snap
            if active:
                point = self.snapToGrid(point)
            return cstr(point)

        else:

            # we have an object to snap to

            obj = FreeCAD.ActiveDocument.getObject(info['Object'])
            if not obj:
                return cstr(point)

            self.lastSnappedObject = obj
                
            if hasattr(obj.ViewObject,"Selectable"):
                if not obj.ViewObject.Selectable:
                    return cstr(point)
                
            if not active:
                
                # passive snapping
                snaps = [self.snapToVertex(info)]

            else:
                
                # active snapping
                comp = info['Component']

                if (Draft.getType(obj) == "Wall") and not oldActive:
                    edges = []
                    for o in [obj]+obj.Additions:
                        if Draft.getType(o) == "Wall":
                            if o.Base:
                                edges.extend(o.Base.Shape.Edges)
                    for edge in edges:
                        snaps.extend(self.snapToEndpoints(edge))
                        snaps.extend(self.snapToMidpoint(edge))
                        snaps.extend(self.snapToPerpendicular(edge,lastpoint))
                        snaps.extend(self.snapToIntersection(edge))
                        snaps.extend(self.snapToElines(edge,eline))
                            
                elif obj.isDerivedFrom("Part::Feature"):
                    if (not self.maxEdges) or (len(obj.Edges) <= self.maxEdges):
                        if "Edge" in comp:
                            # we are snapping to an edge
                            edge = obj.Shape.Edges[int(comp[4:])-1]
                            snaps.extend(self.snapToEndpoints(edge))
                            snaps.extend(self.snapToMidpoint(edge))
                            snaps.extend(self.snapToPerpendicular(edge,lastpoint))
                            #snaps.extend(self.snapToOrtho(edge,lastpoint,constrain)) # now part of snapToPolar
                            snaps.extend(self.snapToIntersection(edge))
                            snaps.extend(self.snapToElines(edge,eline))

                            if isinstance (edge.Curve,Part.Circle):
                                # the edge is an arc, we have extra options
                                snaps.extend(self.snapToAngles(edge))
                                snaps.extend(self.snapToCenter(edge))

                        elif "Vertex" in comp:
                            # directly snapped to a vertex
                            snaps.append(self.snapToVertex(info,active=True))
                        elif comp == '':
                            # workaround for the new view provider
                            snaps.append(self.snapToVertex(info,active=True))
                        else:
                            # all other cases (face, etc...) default to passive snap
                            snapArray = [self.snapToVertex(info)]
                            
                elif Draft.getType(obj) == "Dimension":
                    # for dimensions we snap to their 3 points
                    for pt in [obj.Start,obj.End,obj.Dimline]:
                        snaps.append([pt,'endpoint',pt])
                        
                elif Draft.getType(obj) == "Mesh":
                    # for meshes we only snap to vertices
                    snaps.extend(self.snapToEndpoints(obj.Mesh))

            # updating last objects list
            if not self.lastObj[1]:
                self.lastObj[1] = obj.Name
            elif self.lastObj[1] != obj.Name:
                self.lastObj[0] = self.lastObj[1]
                self.lastObj[1] = obj.Name

            if not snaps:
                return point

            # calculating the nearest snap point
            shortest = 1000000000000000000
            origin = Vector(info['x'],info['y'],info['z'])
            winner = [Vector(0,0,0),None,Vector(0,0,0)]
            for snap in snaps:
                # if snap[0] == None: print "debug: Snapper: 'i[0]' is 'None'"
                delta = snap[0].sub(origin)
                if delta.Length < shortest:
                    shortest = delta.Length
                    winner = snap

            # see if we are out of the max radius, if any
            if self.radius:
                dv = point.sub(winner[2])
                if (dv.Length > self.radius):
                    if (not oldActive) and self.isEnabled("passive"):
                        winner = self.snapToVertex(info)

            # setting the cursors
            if self.tracker:
                self.tracker.setCoords(winner[2])
                self.tracker.setMarker(self.mk[winner[1]])
                self.tracker.on()
            self.setCursor(winner[1])
            
            # return the final point
            return cstr(winner[2])

    def getApparentPoint(self,x,y):
        "returns a 3D point, projected on the current working plane"
        pt = FreeCADGui.ActiveDocument.ActiveView.getPoint(x,y)
        dv = FreeCADGui.ActiveDocument.ActiveView.getViewDirection()
        return FreeCAD.DraftWorkingPlane.projectPoint(pt,dv)
        
    def snapToExtensions(self,point,last,constrain,eline):
        "returns a point snapped to extension or parallel line to last object, if any"

        if self.isEnabled("extension"):
            tsnap = self.snapToExtOrtho(last,constrain,eline)
            if tsnap:
                if (tsnap[0].sub(point)).Length < self.radius:
                    if self.tracker:
                        self.tracker.setCoords(tsnap[2])
                        self.tracker.setMarker(self.mk[tsnap[1]])
                        self.tracker.on()
                    if self.extLine:
                        self.extLine.p2(tsnap[2])
                        self.extLine.on()
                    self.setCursor(tsnap[1])
                    return tsnap[2],eline
                
        for o in [self.lastObj[1],self.lastObj[0]]:
            if o:
                ob = FreeCAD.ActiveDocument.getObject(o)
                if ob:
                    if ob.isDerivedFrom("Part::Feature"):
                        edges = ob.Shape.Edges
                        if (not self.maxEdges) or (len(edges) <= self.maxEdges):
                            for e in edges:
                                if isinstance(e.Curve,Part.Line):
                                    np = self.getPerpendicular(e,point)
                                    if not fcgeo.isPtOnEdge(np,e):
                                        if (np.sub(point)).Length < self.radius:
                                            if self.isEnabled('extension'):
                                                if np != e.Vertexes[0].Point:
                                                    if self.tracker:
                                                        self.tracker.setCoords(np)
                                                        self.tracker.setMarker(self.mk['extension'])
                                                        self.tracker.on()
                                                    if self.extLine:
                                                        self.extLine.p1(e.Vertexes[0].Point)
                                                        self.extLine.p2(np)
                                                        self.extLine.on()
                                                    self.setCursor('extension')
                                                    return np,Part.Line(e.Vertexes[0].Point,np).toShape()
                                        else:
                                            if self.isEnabled('parallel'):
                                                if last:
                                                    de = Part.Line(last,last.add(fcgeo.vec(e))).toShape()  
                                                    np = self.getPerpendicular(de,point)
                                                    if (np.sub(point)).Length < self.radius:
                                                        if self.tracker:
                                                            self.tracker.setCoords(np)
                                                            self.tracker.setMarker(self.mk['parallel'])
                                                            self.tracker.on()
                                                        self.setCursor('parallel')
                                                        return np,de
        return point,eline

    def snapToPolar(self,point,last):
        "snaps to polar lines from the given point"
        if self.isEnabled('ortho'): 
            if last:
                vecs = []
                ax = [FreeCAD.DraftWorkingPlane.u,
                       FreeCAD.DraftWorkingPlane.v,
                       FreeCAD.DraftWorkingPlane.axis]
                for a in self.polarAngles:
                        if a == 90:
                            vecs.extend([ax[0],fcvec.neg(ax[0])])
                            vecs.extend([ax[1],fcvec.neg(ax[1])])
                        else:
                            v = fcvec.rotate(ax[0],math.radians(a),ax[2])
                            vecs.extend([v,fcvec.neg(v)])
                            v = fcvec.rotate(ax[1],math.radians(a),ax[2])
                            vecs.extend([v,fcvec.neg(v)])
                for v in vecs:
                    de = Part.Line(last,last.add(v)).toShape()  
                    np = self.getPerpendicular(de,point)
                    if (np.sub(point)).Length < self.radius:
                        if self.tracker:
                            self.tracker.setCoords(np)
                            self.tracker.setMarker(self.mk['parallel'])
                            self.tracker.on()
                            self.setCursor('ortho')
                        return np,de
        return point,None

    def snapToGrid(self,point):
        "returns a grid snap point if available"
        if self.grid:
            if self.isEnabled("grid"):
                np = self.grid.getClosestNode(point)
                if np:
                    if self.radius != 0:
                        dv = point.sub(np)
                        if dv.Length <= self.radius:
                            if self.tracker:
                                self.tracker.setCoords(np)
                                self.tracker.setMarker(self.mk['grid'])
                                self.tracker.on()
                            self.setCursor('grid')
                            return np
        return point

    def snapToEndpoints(self,shape):
        "returns a list of enpoints snap locations"
        snaps = []
        if self.isEnabled("endpoint"):
            if hasattr(shape,"Vertexes"):
                for v in shape.Vertexes:
                    snaps.append([v.Point,'endpoint',v.Point])
            elif hasattr(shape,"Point"):
                snaps.append([shape.Point,'endpoint',shape.Point])
            elif hasattr(shape,"Points"):
                for v in shape.Points:
                    snaps.append([v.Vector,'endpoint',v.Vector])
        return snaps

    def snapToMidpoint(self,shape):
        "returns a list of midpoints snap locations"
        snaps = []
        if self.isEnabled("midpoint"):
            if isinstance(shape,Part.Edge):
                mp = fcgeo.findMidpoint(shape)
                if mp:
                    snaps.append([mp,'midpoint',mp])
        return snaps

    def snapToPerpendicular(self,shape,last):
        "returns a list of perpendicular snap locations"
        snaps = []
        if self.isEnabled("perpendicular"):
            if last:
                if isinstance(shape,Part.Edge):
                    if isinstance(shape.Curve,Part.Line):
                        np = self.getPerpendicular(shape,last)
                    elif isinstance(shape.Curve,Part.Circle):
                        dv = last.sub(shape.Curve.Center)
                        dv = fcvec.scaleTo(dv,shape.Curve.Radius)
                        np = (shape.Curve.Center).add(dv)
                    elif isinstance(shape.Curve,Part.BSplineCurve):
                        pr = shape.Curve.parameter(last)
                        np = shape.Curve.value(pr)
                    else:
                        return snaps
                    snaps.append([np,'perpendicular',np])
        return snaps

    def snapToOrtho(self,shape,last,constrain):
        "returns a list of ortho snap locations"
        snaps = []
        if self.isEnabled("ortho"):
            if constrain:
                if isinstance(shape,Part.Edge):
                    if last:
                        if isinstance(shape.Curve,Part.Line):
                            if self.constraintAxis:
                                tmpEdge = Part.Line(last,last.add(self.constraintAxis)).toShape()
                                # get the intersection points
                                pt = fcgeo.findIntersection(tmpEdge,shape,True,True)
                                if pt:
                                    for p in pt:
                                        snaps.append([p,'ortho',p])
        return snaps

    def snapToExtOrtho(self,last,constrain,eline):
        "returns an ortho X extension snap location"
        if self.isEnabled("extension") and self.isEnabled("ortho"):
            if constrain and last and self.constraintAxis and self.extLine:
                tmpEdge1 = Part.Line(last,last.add(self.constraintAxis)).toShape()
                tmpEdge2 = Part.Line(self.extLine.p1(),self.extLine.p2()).toShape()
                # get the intersection points
                pt = fcgeo.findIntersection(tmpEdge1,tmpEdge2,True,True)
                if pt:
                    return [pt[0],'ortho',pt[0]]
            if eline:
                try:
                    tmpEdge2 = Part.Line(self.extLine.p1(),self.extLine.p2()).toShape()
                    # get the intersection points
                    pt = fcgeo.findIntersection(eline,tmpEdge2,True,True)
                    if pt:
                        return [pt[0],'ortho',pt[0]]
                except:
                    return None
        return None

    def snapToElines(self,e1,e2):
        "returns a snap location at the infinite intersection of the given edges"
        snaps = []
        if self.isEnabled("intersection") and self.isEnabled("extension"):
            if e1 and e2:
                # get the intersection points
                pts = fcgeo.findIntersection(e1,e2,True,True)
                if pts:
                    for p in pts:
                        snaps.append([p,'intersection',p])
        return snaps
            
    
    def snapToAngles(self,shape):
        "returns a list of angle snap locations"
        snaps = []
        if self.isEnabled("angle"):
            rad = shape.Curve.Radius
            pos = shape.Curve.Center
            for i in [0,30,45,60,90,120,135,150,180,210,225,240,270,300,315,330]:
                ang = math.radians(i)
                cur = Vector(math.sin(ang)*rad+pos.x,math.cos(ang)*rad+pos.y,pos.z)
                snaps.append([cur,'angle',cur])
        return snaps

    def snapToCenter(self,shape):
        "returns a list of center snap locations"
        snaps = []
        if self.isEnabled("center"):
            rad = shape.Curve.Radius
            pos = shape.Curve.Center
            for i in [15,37.5,52.5,75,105,127.5,142.5,165,195,217.5,232.5,255,285,307.5,322.5,345]:
                ang = math.radians(i)
                cur = Vector(math.sin(ang)*rad+pos.x,math.cos(ang)*rad+pos.y,pos.z)
                snaps.append([cur,'center',pos])
        return snaps

    def snapToIntersection(self,shape):
        "returns a list of intersection snap locations"
        snaps = []
        if self.isEnabled("intersection"):
            # get the stored objects to calculate intersections
            if self.lastObj[0]:
                obj = FreeCAD.ActiveDocument.getObject(self.lastObj[0])
                if obj:
                    if obj.isDerivedFrom("Part::Feature"):
                        if (not self.maxEdges) or (len(obj.Shape.Edges) <= self.maxEdges):
                            for e in obj.Shape.Edges:
                                # get the intersection points
                                pt = fcgeo.findIntersection(e,shape)
                                if pt:
                                    for p in pt:
                                        snaps.append([p,'intersection',p])
        return snaps

    def snapToVertex(self,info,active=False):
        p = Vector(info['x'],info['y'],info['z'])
        if active:
            if self.isEnabled("endpoint"):
                return [p,'endpoint',p]
            else:
                return []
        elif self.isEnabled("passive"):
            return [p,'passive',p]
        else:
            return []
        
    def getScreenDist(self,dist,cursor):
        "returns a distance in 3D space from a screen pixels distance"
        p1 = FreeCADGui.ActiveDocument.ActiveView.getPoint(cursor)
        p2 = FreeCADGui.ActiveDocument.ActiveView.getPoint((cursor[0]+dist,cursor[1]))
        return (p2.sub(p1)).Length

    def getPerpendicular(self,edge,pt):
        "returns a point on an edge, perpendicular to the given point"
        dv = pt.sub(edge.Vertexes[0].Point)
        nv = fcvec.project(dv,fcgeo.vec(edge))
        np = (edge.Vertexes[0].Point).add(nv)
        return np

    def setCursor(self,mode=None):
        "setCursor(self,mode=None): sets or resets the cursor to the given mode or resets"
        
        if not mode:
            for v in self.views:
                v.unsetCursor()
            self.views = []
            self.cursorMode = None
        else:
            if mode != self.cursorMode:
                if not self.views:
                    mw = DraftGui.getMainWindow()
                    self.views = mw.findChildren(QtGui.QWidget,"QtGLArea")
                baseicon = QtGui.QPixmap(":/icons/Draft_Cursor.svg")
                newicon = QtGui.QPixmap(32,24)
                newicon.fill(QtCore.Qt.transparent)
                qp = QtGui.QPainter()
                qp.begin(newicon)
                qp.drawPixmap(0,0,baseicon)
                if not (mode == 'passive'):
                    tp = QtGui.QPixmap(self.cursors[mode]).scaledToWidth(16)
                    qp.drawPixmap(QtCore.QPoint(16, 8), tp);
                qp.end()
                cur = QtGui.QCursor(newicon,8,8)
                for v in self.views:
                    v.setCursor(cur)
                self.cursorMode = mode
    
    def off(self):
        "finishes snapping"
        if self.tracker:
            self.tracker.off()
        if self.extLine:
            self.extLine.off()
        if self.grid:
            self.grid.off()
        self.unconstrain()
        self.radius = 0
        self.setCursor()
        if Draft.getParam("hideSnapBar"):
            self.toolbar.hide()

    def constrain(self,point,basepoint=None,axis=None):
        '''constrain(point,basepoint=None,axis=None: Returns a
        constrained point. Axis can be "x","y" or "z" or a custom vector. If None,
        the closest working plane axis will be picked.
        Basepoint is the base point used to figure out from where the point
        must be constrained. If no basepoint is given, the current point is
        used as basepoint.'''

        point = Vector(point)

        # setup trackers if needed
        if not self.constrainLine:
            self.constrainLine = DraftTrackers.lineTracker(dotted=True)

        # setting basepoint
        if not basepoint:
            if not self.basepoint:
                self.basepoint = point
        else:
            self.basepoint = basepoint
        delta = point.sub(self.basepoint)

        # setting constraint axis
        if not self.affinity:
            self.affinity = FreeCAD.DraftWorkingPlane.getClosestAxis(delta)
        if isinstance(axis,FreeCAD.Vector):
            self.constraintAxis = axis
        elif axis == "x":
            self.constraintAxis = FreeCAD.DraftWorkingPlane.u
        elif axis == "y":
            self.constraintAxis = FreeCAD.DraftWorkingPlane.v
        elif axis == "z":
            self.constraintAxis = FreeCAD.DraftWorkingPlane.axis
        else:
            if self.affinity == "x":
                self.constraintAxis = FreeCAD.DraftWorkingPlane.u
            elif self.affinity == "y":
                self.constraintAxis = FreeCAD.DraftWorkingPlane.v
            else:
                self.constraintAxis = FreeCAD.DraftWorkingPlane.axis
                
        # calculating constrained point
        cdelta = fcvec.project(delta,self.constraintAxis)
        npoint = self.basepoint.add(cdelta)

        # setting constrain line
        if self.constrainLine:
            if point != npoint:
                self.constrainLine.p1(point)
                self.constrainLine.p2(npoint)
                self.constrainLine.on()
            else:
                self.constrainLine.off()
		
        return npoint       

    def unconstrain(self):
        self.basepoint = None
        self.affinity = None
        if self.constrainLine:
            self.constrainLine.off()

    def getPoint(self,last=None,callback=None,movecallback=None,extradlg=None):
        
        """
        getPoint([last],[callback],[movecallback],[extradlg]) : gets a 3D point
        from the screen. You can provide an existing point, in that case additional
        snap options and a tracker are available.
        You can also pass a function as callback, which will get called
        with the resulting point as argument, when a point is clicked, and optionally
        another callback which gets called when the mouse is moved.

        If the operation gets cancelled (the user pressed Escape), no point is returned.

        Example:

        def cb(point):
            if point:
                print "got a 3D point: ",point
                
        FreeCADGui.Snapper.getPoint(callback=cb)

        If the callback function accepts more than one argument, it will also receive
        the last snapped object. Finally, a pyqt dialog can be passed as extra taskbox.

        """

        import inspect
        
        self.pt = None
        self.ui = FreeCADGui.draftToolBar
        self.view = FreeCADGui.ActiveDocument.ActiveView

        # setting a track line if we got an existing point
        if last:
            if not self.trackLine:
                self.trackLine = DraftTrackers.lineTracker()
            self.trackLine.p1(last)
            self.trackLine.on()

        def move(event_cb):
            event = event_cb.getEvent()
            mousepos = event.getPosition()
            ctrl = event.wasCtrlDown()
            shift = event.wasShiftDown()
            self.pt = FreeCADGui.Snapper.snap(mousepos,lastpoint=last,active=ctrl,constrain=shift)
            self.ui.displayPoint(self.pt,last,plane=FreeCAD.DraftWorkingPlane,mask=FreeCADGui.Snapper.affinity)
            if self.trackLine:
                self.trackLine.p2(self.pt)
            if movecallback:
                movecallback(self.pt)
        
        def getcoords(point,relative=False):
            self.pt = point
            if relative and last:
                v = FreeCAD.DraftWorkingPlane.getGlobalCoords(point)
                self.pt = last.add(v)
            accept()

        def click(event_cb):
            event = event_cb.getEvent()
            if event.getButton() == 1:
                if event.getState() == coin.SoMouseButtonEvent.DOWN:
                    accept()

        def accept():
            self.view.removeEventCallbackPivy(coin.SoMouseButtonEvent.getClassTypeId(),self.callbackClick)
            self.view.removeEventCallbackPivy(coin.SoLocation2Event.getClassTypeId(),self.callbackMove)
            obj = FreeCADGui.Snapper.lastSnappedObject
            FreeCADGui.Snapper.off()
            self.ui.offUi()
            if self.trackLine:
                self.trackLine.off()
            if callback:
                if len(inspect.getargspec(callback).args) > 2:
                    callback(self.pt,obj)
                else:
                    callback(self.pt)
            self.pt = None

        def cancel():
            self.view.removeEventCallbackPivy(coin.SoMouseButtonEvent.getClassTypeId(),self.callbackClick)
            self.view.removeEventCallbackPivy(coin.SoLocation2Event.getClassTypeId(),self.callbackMove)
            FreeCADGui.Snapper.off()
            self.ui.offUi()
            if self.trackLine:
                self.trackLine.off()
            if callback:
                callback(None)
            
        # adding callback functions
        self.ui.pointUi(cancel=cancel,getcoords=getcoords,extra=extradlg,rel=bool(last))
        self.callbackClick = self.view.addEventCallbackPivy(coin.SoMouseButtonEvent.getClassTypeId(),click)
        self.callbackMove = self.view.addEventCallbackPivy(coin.SoLocation2Event.getClassTypeId(),move)

    def makeSnapToolBar(self):
        "builds the Snap toolbar"
        self.toolbar = QtGui.QToolBar(None)
        self.toolbar.setObjectName("Draft Snap")
        self.toolbar.setWindowTitle("Draft Snap")
        self.toolbarButtons = []
        self.masterbutton = QtGui.QPushButton(None)
        self.masterbutton.setIcon(QtGui.QIcon(":/icons/Snap_Lock.svg"))
        self.masterbutton.setIconSize(QtCore.QSize(16, 16))
        self.masterbutton.setMaximumSize(QtCore.QSize(26,26))
        self.masterbutton.setToolTip("Snap On/Off")
        self.masterbutton.setObjectName("SnapButtonMain")
        self.masterbutton.setCheckable(True)
        self.masterbutton.setChecked(True)
        QtCore.QObject.connect(self.masterbutton,QtCore.SIGNAL("toggled(bool)"),self.toggle)
        self.toolbar.addWidget(self.masterbutton)
        for c,i in self.cursors.iteritems():
            if i:
                b = QtGui.QPushButton(None)
                b.setIcon(QtGui.QIcon(i))
                b.setIconSize(QtCore.QSize(16, 16))
                b.setMaximumSize(QtCore.QSize(26,26))
                b.setToolTip(c)
                b.setObjectName("SnapButton"+c)
                b.setCheckable(True)
                b.setChecked(True)
                self.toolbar.addWidget(b)
                self.toolbarButtons.append(b)
        if not Draft.getParam("showSnapBar"):
            self.toolbar.hide()

    def toggle(self,checked=None):
        if hasattr(self,"toolbarButtons"):
            if checked == None:
                self.masterbutton.toggle()
            elif checked:
                if hasattr(self,"savedButtonStates"):
                    for i in range(len(self.toolbarButtons)):
                        self.toolbarButtons[i].setEnabled(True)
                        self.toolbarButtons[i].setChecked(self.savedButtonStates[i])
            else:
                self.savedButtonStates = []
                for i in range(len(self.toolbarButtons)):
                    self.savedButtonStates.append(self.toolbarButtons[i].isChecked())
                    self.toolbarButtons[i].setEnabled(False)

    def isEnabled(self,but):
        "returns true if the given button is turned on"
        for b in self.toolbarButtons:
            if str(b.objectName()) == "SnapButton" + but:
                return (b.isEnabled() and b.isChecked())
        return False

    def show(self):
        "shows the toolbar"
        if not hasattr(self,"toolbar"):
            self.makeSnapToolBar()
        mw = getMainWindow()
        bt = mw.findChild(QtGui.QToolBar,"Draft Snap")
        if not bt:
            mw.addToolBar(self.toolbar)
        self.toolbar.show()

if not hasattr(FreeCADGui,"Snapper"):
    FreeCADGui.Snapper = Snapper()
