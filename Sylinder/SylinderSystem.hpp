/**
 * @file SylinderSystem.hpp
 * @author wenyan4work (wenyan4work@gmail.com)
 * @brief System for sylinders
 * @version 1.0
 * @date 2018-12-13
 *
 * @copyright Copyright (c) 2018
 *
 */
#ifndef SYLINDERSYSTEM_HPP_
#define SYLINDERSYSTEM_HPP_

#include "Sylinder.hpp"
#include "SylinderConfig.hpp"
#include "SylinderNear.hpp"

#include "Collision/CollisionSolver.hpp"
#include "FDPS/particle_simulator.hpp"
#include "Trilinos/TpetraUtil.hpp"
#include "Util/TRngPool.hpp"

/**
 * @brief A collection of sylinders distributed to multiple MPI ranks.
 *
 */
class SylinderSystem {
    int snapID;    ///< the current id of the snapshot file to be saved. sequentially numbered from 0
    int stepCount; ///< timestep Count. sequentially numbered from 0

    // FDPS stuff
    PS::DomainInfo dinfo; ///< domain size, boundary condition, and decomposition info
    void setDomainInfo();

    PS::ParticleSystem<Sylinder> sylinderContainer;        ///< sylinders
    std::unique_ptr<TreeSylinderNear> treeSylinderNearPtr; ///< short range interaction of sylinders
    int treeSylinderNumber;                                ///< the current max_glb number of treeSylinderNear
    void setTreeSylinder();

    // Collision stuff
    std::shared_ptr<CollisionSolver> collisionSolverPtr;       ///< pointer to CollisionSolver object
    std::shared_ptr<CollisionCollector> collisionCollectorPtr; ///<  pointer to CollisionCollector object
    Teuchos::RCP<TV> forceNonBrownRcp;                         ///< Non Brownian force, set by user
    Teuchos::RCP<TV> velocityNonBrownRcp;                      ///< Non Brownian velocity, set by user
    Teuchos::RCP<TV> velocityBrownRcp;                         ///< Brownian velocity, generated by calcBrown

    // used in collision solver
    Teuchos::RCP<TV> velocityKnownRcp; ///< \f$V_{known} = V_{Brown}+V_{NonBrown}+M F_{NonBrown}\f$
    Teuchos::RCP<TV> forceColRcp;      ///< collision force solution
    Teuchos::RCP<TV> velocityColRcp;   ///< collision velocity solution

    // MPI stuff
    std::shared_ptr<TRngPool> rngPoolPtr;      ///< thread safe TRngPool
    Teuchos::RCP<const TCOMM> commRcp;         ///< TCOMM
    Teuchos::RCP<TMAP> sylinderMapRcp;         ///< TMAP, 1 dof per sylinder
    Teuchos::RCP<TMAP> sylinderMobilityMapRcp; ///< TMAP, 6 dofs per sylinder
    void updateSylinderMap();                  ///< update sylindermap and sylinderMobilityMap

    Teuchos::RCP<TCMAT> mobilityMatrixRcp; ///< block-diagonal mobility matrix
    Teuchos::RCP<TOP> mobilityOperatorRcp; ///< full mobility operator (matrix-free), to be implemented

    // internal utility functions
    void setInitialFromConfig();
    void setInitialFromFile(const std::string &filename);
    void setInitialCircularCrossSection();

    void showOnScreenRank0();

    void writeVTK(const std::string &baseFolder);
    void writeAscii(const std::string &baseFolder);
    void writeBox();

    void setPosWithWall(); // directly set the position of sylinders according to wall

    std::pair<int, int> getMaxGid();

    void getOrient(Equatn &orient, const double px, const double py, const double pz, const int threadId);
    void getRandPointInCircle(const double &radius, double &x, double &y, const int &threadId);
    void fitInPeriodicBound(double &x, const double &lb, const double &ub) const;

  public:
    SylinderConfig runConfig; // Be careful if this is modified on the fly

    SylinderSystem() = default;
    SylinderSystem(const std::string &configFile, const std::string &posFile, int argc, char **argv);
    SylinderSystem(const SylinderConfig &config, const std::string &posFile, int argc, char **argv);
    ~SylinderSystem() = default;
    // forbid copy
    SylinderSystem(const SylinderSystem &) = delete;
    SylinderSystem &operator=(const SylinderSystem &) = delete;

    void initialize(const SylinderConfig &config, const std::string &posFile, int argc, char **argv);

    void calcBoundingBox(double localLow[3], double localHigh[3], double globalLow[3], double globalHigh[3]);

    void decomposeDomain();  // domain decomposition must be triggered when particle distribution significantly changes
    void exchangeSylinder(); // particle exchange must be triggered every timestep:

    /**
     * one-step high level API
     */
    // get information
    PS::ParticleSystem<Sylinder> &getContainer() { return sylinderContainer; }
    PS::DomainInfo &getDomainInfo() { return dinfo; }
    std::shared_ptr<TRngPool> &getRngPoolPtr() { return rngPoolPtr; }
    Teuchos::RCP<const TCOMM> &getCommRcp() { return commRcp; }

    // between prepareStep() and runStep(), sylinders should not be moved, added, or removed
    void prepareStep();
    void setForceNonBrown(const std::vector<double> &forceNonBrown);
    void setVelocityNonBrown(const std::vector<double> &velNonBrown);
    void runStep();

    // These should run after runStep()
    void addNewSylinder(std::vector<Sylinder> &newSylinder); // add new particles and assign new (unique) gid
    void calcColStress();                                    // calc collision stress
    void calcVolFrac();                                      // calc volume fraction

    /**
     * detailed low level API
     */
    // apply simBox BC, before each step
    void applyBoxBC();

    // compute non-collision velocity and mobility, before collision resolution
    void calcVelocityBrown(); // write back to sylinder.velBrown
    void calcVelocityKnown(); // write back to sylinder.velNonB before adding velBrown
    void calcMobMatrix();     // calculate the mobility matrix
    void calcMobOperator();   // calculate the mobility operator

    // resolve collision
    void collectWallCollision();  // collect wall collision constraints
    void collectPairCollision();  // collect pair collision constraints
    void resolveCollision();      // resolve collision
    void saveVelocityCollision(); // write back to sylinder.velCol

    // move with both collision and non-collision velocity
    void stepEuler(); // Euler step update position and orientation, with given velocity

    // write results
    std::string getCurrentResultFolder(); // get the current output folder path
    bool getIfWriteResultCurrentStep();   // check if the current step is writing (set by runConfig)
    int getSnapID() { return snapID; };
    void writeResult(); // write result regardless of runConfig

    // expose raw vectors and operators
    Teuchos::RCP<TV> getForceNonBrown() const { return forceNonBrownRcp; }
    Teuchos::RCP<TV> getVelocityNonBrown() const { return velocityKnownRcp; };
    Teuchos::RCP<TV> getVelocityBrown() const { return velocityBrownRcp; };
    Teuchos::RCP<TV> getVelocityKnown() const { return velocityKnownRcp; };
    Teuchos::RCP<TV> getForceCol() const { return forceColRcp; };
    Teuchos::RCP<TV> getVelocityCol() const { return velocityColRcp; };
    Teuchos::RCP<TCMAT> getMobMatrix() { return mobilityMatrixRcp; };
    Teuchos::RCP<TOP> getMobOperator() { return mobilityOperatorRcp; };
};

#endif