boardWidth = 30;
boardLength = 75;
boardThickness = 1.6;
wallThickness = 1.5;
gap = .5;
caseHeight = 25;
caseLength = boardLength+wallThickness+gap;
caseWidth = boardWidth+2*wallThickness-2*gap+.4;
buttonRad = 5;
cutWidth = 1;

slop = 0.4;
difference()
{
    cube([caseLength,caseWidth,caseHeight]);
    // carve out general insides
    translate([wallThickness,wallThickness,wallThickness])
    cube([caseLength-wallThickness,caseWidth-2*wallThickness,caseHeight]);
    // carve out slots for board
    translate([wallThickness,0.5*(caseWidth-boardWidth)-slop,caseHeight-3])
    cube([caseLength,boardWidth+2*slop,boardThickness+slop]);
    // carve out sensor wire slot
    translate([0,caseWidth-12-wallThickness,caseHeight-8])
    cube([wallThickness,10,8+boardThickness]);   
    // carve out button press 
    translate([22+1,caseWidth-21,0])
    {
        difference()
        {
            cylinder(r=buttonRad+cutWidth,h=boardThickness,$fn=20);
            cylinder(r=buttonRad, h=boardThickness,$fn=20);
            translate([0,-.5*buttonRad,0])
            cube([2*buttonRad,buttonRad,boardThickness]);
        }
        
        translate([sqrt(3)/2*buttonRad,-0.5*buttonRad-cutWidth,0])
        cube([20,cutWidth,boardThickness]);
        translate([sqrt(3)/2*buttonRad,0.5*buttonRad,0])
        cube([20,cutWidth,boardThickness]);
    }
    
    // carve out LED view hole
    
    translate([22+1,caseWidth-6-wallThickness,0])
    cylinder(r=3,h=boardThickness,$fn=20);
}

translate([wallThickness+22,caseWidth-21,boardThickness])
cylinder(d1=5,d2=3,h=caseHeight-12,$fn=20);


