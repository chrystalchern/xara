#file examples\soil\MY0.tcl  
#plane strain,  single element,  static analysis,  SI units (m, s, KN, ton)
#Note: Static analysis only works with Linear loading time series, see OpenSees Primer.
#         4     3
#         ------- --> F  (loads applied to node 3)
#         |     |
#         |     |
#         |     |
#        1-------2   (nodes 1 and 2 fixed)
#         ^     ^
puts "\n PLEASE SEND QUESTIONS TO zhyang@ucsd.edu (Zhaohui Yang) \n"
#
#some user defined variables
# 
set matOpt   3      ;# 1 = drained, pressure depend;  2 = undrained, pressure depend; 
                    ;# 3 = undrained, pressure independ; 4 = elastic 
set massDen  0      ;# mass density
set press   -80.    ;# isotropic consolidation pressure on quad element(s)
set loadInc  0.45    ;# load increment per step for monotonic loading option
set numAnalysisSteps 3
set version  2      ;# OpenSees version (1=> version 1.1.1, 2=> version 1.1.2 and above)

#############################################################
# BUILD MODEL

#create the ModelBuilder
model basic -ndm 2 -ndf 2

# define material and properties
switch $matOpt {
  1 {
    nDMaterial PressureDependMultiYield 1 2 4.e4 2.e5 30. .1 80 1 0.5 2 \
                                        22.0 0.05 -0.05 0.02 5 0.04 0 0.0 0.0 0. 101
    if {$version==1} {
       updateMaterialStage material 1 stage 0
    } else {
       updateMaterialStage -material 1 -stage 0
    }
    set eleMat "PlaneStrain"
    set gravY [expr -9.81*$massDen]  ;#gravity
  }
  2 {
    nDMaterial PressureDependMultiYield 1 2 4.e4 2.e5 30. .1 80 1 0.5 20 \
                                        22.0 0.05 -0.05 0.02 5 0.04 10 0.01 3.0 1. 101
    nDMaterial FluidSolidPorous 2 2 1 2.2e6 101
    if {$version==1} {
       updateMaterialStage material 1 stage 0
       updateMaterialStage material 2 stage 0
    } else {
       puts "asfasf"
       updateMaterialStage -material 1 -stage 0
       updateMaterialStage -material 2 -stage 0
    }
    set eleMat "PlaneStrain"
    set gravY [expr -9.81*($massDen-1.0)]  ;# buoyant unit weight
  }
  3 {
    nDMaterial PressureIndependMultiYield 1 2 3.e4 4.e5  0. .1 80 40 0.0 20 
    nDMaterial FluidSolidPorous 3 2 1 2.2e6 101
    if {$version==1} {
       updateMaterialStage material 1 stage 0
       updateMaterialStage material 3 stage 0
    } else {
       updateMaterialStage -material 1 -stage 0
       updateMaterialStage -material 3 -stage 0
    }
    set eleMat "PlaneStrain"
    set gravY [expr -9.81*($massDen-1.0)]  ;# buoyant unit weight
  }
  4 {
    nDMaterial ElasticIsotropic 4 11.25e4 0.40625
    set eleMat "PlaneStrain2D"
    set gravY [expr -9.81*$massDen]  ;#gravity
  }
}

# define the nodes
node 1   0.0 0.0 
node 2   1.0 0.0 
node 3   1.0 1.0 
node 4   0.0 1.0


# define the element      thick  material   maTag  press    mDensity   gravity 
element quad  1  1 2 3 4  1.0   $eleMat   $matOpt  $press   $massDen   0 $gravY  

# fix the base 
fix 1 1 1 
fix 2 0 1

#############################################################
# GRAVITY APPLICATION (elastic behavior)

# create the SOE, ConstraintHandler, Integrator, Algorithm and Numberer
# system SparseGeneral
#test NormUnbalance 1.0e-3 10 0
# algorithm Newton
system ProfileSPD
test NormDispIncr 1.0e-4 35 0
algorithm Linear
constraints Penalty 1.e14 1.e14
integrator LoadControl 1 1 1 1
numberer RCM

# create the Analysis
analysis Static 

analyze 2

# switch material stage from elastic (gravity) to plastic
if {$version==1} {
  switch $matOpt {
   1 {
    updateMaterialStage material 1 stage 1
   }
   2 {
    updateMaterialStage material 1 stage 1
    updateMaterialStage material 2 stage 1
   }
   3 {
    updateMaterialStage material 1 stage 1
    updateMaterialStage material 3 stage 1
   }
   4  # do nothing
  }
} else {
  switch $matOpt {
   1 {
    updateMaterialStage -material 1 -stage 1
   }
   2 {
    updateMaterialStage -material 1 -stage 1
    updateMaterialStage -material 2 -stage 1
   }
   3 {
    updateMaterialStage -material 1 -stage 1
    updateMaterialStage -material 3 -stage 1
   }
   4  # do nothing
  } 
}

#############################################################
# NOW APPLY LOADING SEQUENCE AND ANALYZE (plastic)

# rezero time
setTime 0.0

equalDOF 1 2   1      ;#tie nodes 1 and 2
equalDOF 3 4   1 2    ;#tie nodes 3 and 4

# create a LoadPattern with a Linear time series
  pattern Plain 1 Linear {
    load 3 $loadInc 0.0    ;#load incre
  }

#create the recorder
recorder Node nodalDisp disp -time -node 3 4 -dof 1 2 
recorder Element 1 -time -file stress1 material 1 stress
recorder Element 1 -time -file strain1 material 1 strain
recorder Element 1 -time -file stress2 material 2 stress
recorder Element 1 -time -file strain2 material 2 strain
recorder Element 1 -time -file stress3 material 3 stress
recorder Element 1 -time -file strain3 material 3 strain
recorder Element 1 -time -file stress4 material 4 stress
recorder Element 1 -time -file strain4 material 4 strain
if { $matOpt == 2 || $matOpt == 3 } {   ;#excess pore pressure output
  recorder Element 1 -time -file press1 material 1 pressure
  recorder Element 1 -time -file press2 material 2 pressure
  recorder Element 1 -time -file press3 material 3 pressure
  recorder Element 1 -time -file press4 material 4 pressure
}

# create the Analysis
algorithm ModifiedNewton
analysis Static 
#analyze the structure
analyze $numAnalysisSteps

wipe  #flush output stream



