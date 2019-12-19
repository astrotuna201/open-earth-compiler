// RUN: oec-opt %s --stencil-call-inlining | oec-opt | FileCheck %s

func @load(%in : !stencil.view<ijk,f64>) -> f64
  attributes { stencil.function } {
	%0 = stencil.access %in[ 0, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	return %0 : f64
}

func @lap(%in : !stencil.view<ijk,f64>) -> f64
  attributes { stencil.function } {
	%0 = stencil.call @load(%in)[-1, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	%1 = stencil.call @load(%in)[ 1, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	%2 = stencil.call @load(%in)[ 0, 1, 0] : (!stencil.view<ijk,f64>) -> f64
	%3 = stencil.call @load(%in)[ 0,-1, 0] : (!stencil.view<ijk,f64>) -> f64
	%4 = stencil.call @load(%in)[ 0, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	%5 = addf %0, %1 : f64
	%6 = addf %2, %3 : f64
	%7 = addf %5, %6 : f64
	%8 = constant -4.0 : f64
	%9 = mulf %4, %8 : f64
	%10 = addf %9, %7 : f64
	return %10 : f64
}

// CHECK-LABEL: func @lap(%{{.*}}: !stencil.view<ijk,f64>) -> f64 attributes {stencil.function} {
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}[-1, 0, 0] : (!stencil.view<ijk,f64>) -> f64
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}[1, 0, 0] : (!stencil.view<ijk,f64>) -> f64
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}[0, 1, 0] : (!stencil.view<ijk,f64>) -> f64
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}[0, -1, 0] : (!stencil.view<ijk,f64>) -> f64
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}[0, 0, 0] : (!stencil.view<ijk,f64>) -> f64

func @laplap(%in : !stencil.view<ijk,f64>) -> f64
  attributes { stencil.function } {
	%0 = stencil.call @lap(%in)[-1, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	%1 = stencil.call @lap(%in)[ 1, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	%2 = stencil.call @lap(%in)[ 0, 1, 0] : (!stencil.view<ijk,f64>) -> f64
	%3 = stencil.call @lap(%in)[ 0,-1, 0] : (!stencil.view<ijk,f64>) -> f64
	%4 = stencil.call @lap(%in)[ 0, 0, 0] : (!stencil.view<ijk,f64>) -> f64
	%5 = addf %0, %1 : f64
	%6 = addf %2, %3 : f64
	%7 = addf %5, %6 : f64
	%8 = constant -4.0 : f64
	%9 = mulf %4, %8 : f64
	%10 = addf %9, %7 : f64
	return %10 : f64
}

// CHECK-LABEL: func @laplap(%{{.*}}: !stencil.view<ijk,f64>) -> f64 attributes {stencil.function} {
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}
//  CHECK-NEXT: %{{.*}} = stencil.access %{{.*}}

func @laplap_stencil(%in: !stencil.field<ijk,f64>, %out: !stencil.field<ijk,f64>)
  attributes { stencil.program } {
	%0 = stencil.load %in : (!stencil.field<ijk,f64>) -> !stencil.view<ijk,f64>
	%1 = stencil.apply @laplap(%0) : (!stencil.view<ijk,f64>) -> !stencil.view<ijk,f64>
	stencil.store %1 to %out[0, 0, 0][64, 64, 60] : !stencil.view<ijk,f64> to !stencil.field<ijk,f64>
	return
}

// CHECK-LABEL: func @laplap_stencil(%{{.*}}: !stencil.field<ijk,f64>, %{{.*}}: !stencil.field<ijk,f64>) attributes {stencil.program}
//  CHECK-NEXT: %{{.*}} = stencil.load %{{.*}} : (!stencil.field<ijk,f64>) -> !stencil.view<ijk,f64>
//  CHECK-NEXT: %{{.*}} = stencil.apply @laplap(%{{.*}}) : (!stencil.view<ijk,f64>) -> !stencil.view<ijk,f64>
//  CHECK-NEXT: stencil.store %{{.*}} to %{{.*}} : !stencil.view<ijk,f64> to !stencil.field<ijk,f64>
