magnetLength = 30;
magnetWidth = 15;
thickness = 2;
flangeWidth = 20;
screwOD = 7;
screwID = 4;
$fn=20;

difference()
{
    cube([magnetLength,flangeWidth,thickness]);
    translate([0,flangeWidth/2,0])
    {
        translate([magnetLength/4,0,0])
        cylinder(d1=screwID,d2=screwOD,thickness);
        translate([3*magnetLength/4,0,0])
        cylinder(d1=screwID,d2=screwOD,thickness);
    }
}
translate([0,-thickness,0])
cube([magnetLength,thickness,magnetWidth]);
