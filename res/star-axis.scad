// 5-pointed star with solid infill - animated
// Outer radius R, inner radius r = R * sin(18) / sin(54)
// To animate: View → Animate, set FPS=60, Steps=120 (2 sec duration)

R = 10; // outer radius

// Calculate inner radius using the golden ratio proportion
r = R * sin(18) / sin(54);

function star_point(i, R, r) =
    i % 2 == 0
        ? [R * cos(90 - i * 36), R * sin(90 - i * 36)]
        : [r * cos(90 - i * 36), r * sin(90 - i * 36)];

points = [for (i = [0:9]) star_point(i, R, r)];

color("#ffff00")
rotate([0, 360 * $t, 0])
scale(1 - $t * 0.999)
    linear_extrude(height = 2)
        polygon(points);
