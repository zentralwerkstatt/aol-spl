// IN ADDON FILE ofxCsv.cpp:
// REPLACED LINE 87: fileIn.open(path.c_str()); WITH fileIn.open(ofToDataPath(path, true).c_str());

#include "ofApp.h"
#include "ofxCsv.h"


int globalY = 0;

int dayFactor = 2;
int hourFactor = 6;
int minQueries = 10;

int boxVertexCount;
int globalIndexCount = 0;

string hudTextSystems = "";
string hudTextPillars = "";

ofTrueTypeFont font3D;
ofTrueTypeFont fontHud;
ofMesh theMesh;
ofEasyCam cam;


class Pillar {
    
public:
    ofVec3f position;
    string shadow;
    float h;
    
    Pillar(ofVec3f pPosition, string pShadow, float pH) {
        position = pPosition;
        shadow = pShadow;
        h = pH;
    }
    
    void draw() {
        ofPushMatrix();
            ofSetColor(ofColor::fromHsb(170,100,150,255));
            ofDrawLine(position.x,position.y,position.z,position.x,position.y+h,position.z);
        ofPopMatrix();
    }
    
};



class Query {
    
public:
    string content;
    int userID,day,hour,deweyClass;
    
    Query(int pUserID, string pContent, int pDay, int pHour) {
        userID = pUserID;
        content = pContent;
        day = pDay;
        hour = pHour;
    }
    
};


class Satellite {
    
public:
    string shadow;
    ofVec3f position;
    ofVec3f centroid;
    bool active = false;
    
    Satellite(ofVec3f pPosition, string pShadow) {
        position = pPosition;
        shadow = pShadow;
    }
    
    void setCentroid(ofVec3f pCentroid) {
        centroid = pCentroid;
    }
    
    void draw() {
        
        ofPushMatrix();
            ofSetColor(ofColor(50,50,50,255));
            ofTranslate(position.x,position.y,position.z);
            ofScale(0.1,0.1,0);
            font3D.drawString(shadow,0,0);
        ofPopMatrix();
        
    }
    
};


vector<Pillar> pillars;


class SatelliteSystem {
    
public:
    vector<Satellite> satellites;
    ofVec3f centroid;
    int userID;
    int internalID;
    float maxDist = 0;
    bool active = false;
    
    SatelliteSystem(int pUserID, vector<Query> pQueries) {
        
        userID = pUserID;
        globalY += 1;
        float y = globalY;
        
        // create satellites
        for (Query q : pQueries) {
            ofVec3f v;
            v.set(q.day,y,q.hour);
            centroid += v;
            satellites.push_back(Satellite(v,q.content));
        }
        centroid /= pQueries.size();

        // find maximum distance for bounding circle
        for (Satellite &s : satellites) {
            s.setCentroid(centroid);
            float d = centroid.distance(s.position);
            if (d > maxDist) {maxDist = d;}
        }
        
        float boxSize = pQueries.size()/10;
        ofMesh tempBox = ofMesh::sphere(boxSize);
        
        vector<ofVec3f> vertexVec = tempBox.getVertices();
        vector<ofIndexType> indexVec = tempBox.getIndices();
        
        boxVertexCount = vertexVec.size();

        for (ofVec3f &v : vertexVec) {theMesh.addVertex(centroid+v);}
        for (ofIndexType &i : indexVec) {theMesh.addIndex(globalIndexCount+i);}
        
        // important, add the number of VERTICES to the global index of INDICES (not the number of indices), as there are more vertices than indices
        globalIndexCount += boxVertexCount;
        
        
    }
    
    void draw(ofVec3f pPreviousCentroid) {
        
        // line to previous satellite system
        ofPushMatrix();
            ofSetColor(ofColor::fromHsb(0,100,255,255));
            ofDrawLine(centroid,pPreviousCentroid);
        ofPopMatrix();
        
        if (active) {
            
            hudTextSystems = "";
            hudTextPillars = "";
            
            // bounding circle
            ofPushMatrix();
                ofNoFill();
                ofSetColor(ofColor::fromHsb(0,100,255,255));
                ofTranslate(centroid);
                ofRotateX(90);
                ofDrawCircle(0,0,maxDist*2);
            ofPopMatrix();
            
            for (Satellite &s : satellites) {

                ofSetColor(ofColor::fromHsb(170,100,150,255));
                
                // line to satellite
                ofPushMatrix();
                    ofDrawLine(s.position,centroid);
                ofPopMatrix();
                
                hudTextSystems += s.shadow + "\n";
                
                // draw only pillars that have their position in common with a satellite
                for (Pillar &p : pillars) {
                    if (p.position.x == s.position.x && p.position.z == s.position.z) {
                        p.draw();
                        hudTextPillars += p.shadow  + "\n";
                    }
                }
                
            }
            
            hudTextSystems += "--- User " + ofToString(userID);
            
        }
        
    }
    
};


vector<Query> data;
vector<SatelliteSystem> systems;


void ofApp::setup(){
    
    // general options
    ofHideCursor();
    ofEnableSmoothing();
    ofSetFrameRate(60);
    ofEnableAlphaBlending();
    ofEnableDepthTest();
    
    // prepare the fonts
    ofTrueTypeFont::setGlobalDpi(72);
    font3D.load("courier_new.ttf",50,true,true,true);
    fontHud.load("courier_new.ttf",12,true,true,true);

    // prepare the mesh
    theMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    theMesh.disableColors();
    theMesh.enableIndices();
    
    // load data
    wng::ofxCsv tableAOL;
    wng::ofxCsv tableSPL;
    tableAOL.loadFile("aol_data.csv");
    tableSPL.loadFile("spl_data.csv");
    int rowsAOL = tableAOL.numRows;
    int colsAOL = tableAOL.numCols;
    int rowsSPL = tableSPL.numRows;
    int colsSPL = tableSPL.numCols;
    
    // populate data array
    for (int i=0; i<rowsAOL; i++) {
        data.push_back(Query(tableAOL.getInt(i,0),tableAOL.getString(i,1),tableAOL.getInt(i,2)*dayFactor,tableAOL.getInt(i,3)*hourFactor));
    }
        
    // populate satellite systems
    int lastNewUserID = data.at(0).userID;
    int currentUserID;
    vector<Query> queries;
    
    for (Query &d : data) {
        currentUserID = d.userID;
        if (currentUserID == lastNewUserID) {queries.push_back(d);}
        else {
            queries.push_back(d);
            if (queries.size() >= minQueries) {
                systems.push_back(SatelliteSystem(lastNewUserID,queries));
            }
            lastNewUserID = currentUserID;
            queries.clear();
        }
    }
    
    cout << systems.size() << " systems\n";
        
    // populate pillars
    for (int i=0; i<rowsSPL; i++) {
        pillars.push_back(Pillar(ofVec3f(tableSPL.getInt(i,1)*dayFactor,0,tableSPL.getInt(i,2)*hourFactor),tableSPL.getString(i,0),globalY));
    }
    
    cout << pillars.size() << " pillars\n";
    
    // create normals for mesh after it is filled with vertices (from inside the SatelliteSystem class)
    theMesh.addNormals(theMesh.getFaceNormals());

}


void ofApp::draw(){
    
    ofClear(255,255,250,255);
    
    cam.begin();
    
    ofSetColor(ofColor(100,100,100,255));
    theMesh.drawWireframe();
    
        ofVec3f previousCentroid = ofVec3f(0,0,0);
        for (int i=0; i < systems.size(); i++) {
            systems.at(i).draw(previousCentroid);
            previousCentroid = systems.at(i).centroid;
            
            if (systems.at(i).active) {
                for (Satellite &currentSatellite : systems.at(i).satellites) {
                    currentSatellite.draw();
                }
            }
        }

    cam.end();

    ofSetColor(ofColor(255,255,255,255));

    // HUD
    ofSetColor(ofColor(0,0,0,180));
    fontHud.drawString(hudTextSystems,10,20);
    
    ofSetColor(ofColor::fromHsb(0,100,255,255));
    ofRectangle lowerHudRect = fontHud.getStringBoundingBox	(hudTextPillars,0,0);
    int lowerHudX = 10;
    int lowerHudY = ofGetHeight() - 10 - lowerHudRect.getHeight();
    fontHud.drawString(hudTextPillars,lowerHudX,lowerHudY);
    
}


void ofApp::mouseMoved(int x, int y ){
    // to avoid the cursor being hidden (bug in OSX Yosemite, see http://forum.openframeworks.cc/t/mouse-cursor-invisible-in-osx/17929)
    ofShowCursor();
}


void ofApp::mousePressed(int x, int y, int button){
    
    // 3D picking
    int n = theMesh.getNumVertices();
    float nearestDistance = 0;
    ofVec2f nearestVertex;
    int nearestIndex = 0;
    ofVec2f mouse(mouseX, mouseY);
    for(int i = 0; i < n; i++) {
        ofVec3f cur = cam.worldToScreen(theMesh.getVertex(i));
        float distance = cur.distance(mouse);
        if(i == 0 || distance < nearestDistance) {
            nearestDistance = distance;
            nearestVertex = cur;
            nearestIndex = i;
        }
    };
    
    cout << "Index clicked: " << nearestIndex/boxVertexCount << "\n";
    
    for(SatelliteSystem &s : systems) {
        s.active = false;
    }
    
    systems.at(nearestIndex/boxVertexCount).active = true;
    
}


// unused openFrameworks template functions
void ofApp::mouseReleased(int x, int y, int button){}
void ofApp::mouseDragged(int x, int y, int button){}
void ofApp::keyPressed(int key){}
void ofApp::update(){}
void ofApp::keyReleased(int key){}
void ofApp::windowResized(int w, int h){}
void ofApp::gotMessage(ofMessage msg){}
void ofApp::dragEvent(ofDragInfo dragInfo){ }
