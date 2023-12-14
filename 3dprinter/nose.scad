

module pin(has_nop = true){
        translate([0,0,-2]){
        difference(){
            cylinder(2,27/2,27/2);
            translate([2,2,-6])
                cube([40-0,40,10]);            
            translate([-2-40,2,-6])
                cube([45,40,10]);            
            translate([-2-40,-2-40,-6])
                cube([40,45,10]);            
            translate([2,-2-40,-6])
                cube([40,45,10]);            

            translate([0,0,-1])
                cylinder(5,27/2-1,27/2-1);            
        }
        if (has_nop){
            translate([0,0,0])
            difference(){
                cylinder(1,27/2+1,27/2+1);
                translate([0,2,-6])
                    cube([40,40,10]);
                translate([-40,2,-6])
                    cube([45,40,10]);
                translate([-2-40,-2-40,-6])
                    cube([40,45,10]);
                translate([2,-2-40,-6])
                    cube([40,45,10]);        
                translate([0,0,-1])
                    cylinder(5,26/2,26/2);
            }
        }
    }
}

module cone(){
    difference(){
        cylinder(12,27/2,27/2);
        cylinder(10,27/2-1,27/2-1);
    }
    rotate([0,0,90*0+5])
        pin(true);
    rotate([0,0,90*1-5])
        pin(true);
    rotate([0,0,90*2+5])
        pin(false);
    rotate([0,0,90*3-5])
        pin(true);    
}



difference(){
    cone();
    rotate(45+90){
        translate([-5.25,2.25,8]){
            cube([10.5,6.5,8]);
            translate([-1.25,2.5,0])
            cube([13,2,3]);
        }
    }
}
