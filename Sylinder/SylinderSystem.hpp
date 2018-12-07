#ifndef SYLINDERSYSTEM_HPP_
#define SYLINDERSYSTEM_HPP_

#include "Sylinder.hpp"
#include "SylinderConfig.hpp"
#include "SylinderNear.hpp"

#include "Collision/CollisionSolver.hpp"
#include "MPI/FDPS/particle_simulator.hpp"
#include "Trilinos/TpetraUtil.hpp"
#include "Util/TRngPool.hpp"

#include <algorithm>
#include <memory>

class SylinderSystem {

  public:
    const SylinderConfig runConfig;
    SylinderSystem(const std::string &configFile, const std::string &posFile, int argc, char **argv);
    SylinderSystem(const SylinderConfig &config, const std::string &posFile, int argc, char **argv);
    ~SylinderSystem() = default;
    // forbid copy
    SylinderSystem(const SylinderSystem &) = delete;
    SylinderSystem &operator=(const SylinderSystem &) = delete;

    // domain decomposition and load balancing
    void partition();

    // IO
    std::string getCurrentOutputFolder();
    void output();
    void writeVTK(const std::string &baseFolder);
    void writeXYZ(const std::string &baseFolder);

    // one-step high level API
    void prepareStep();
    void step();

    // detailed low level API
    PS::ParticleSystem<Sylinder> &getContainer() { return sylinderContainer; }
    PS::DomainInfo &getDomainInfo() { return dinfo; }
    void getBoundingBox(Evec3 &localLow, Evec3 &localHigh, Evec3 &globalLow, Evec3 &globalHigh);
    void applyBoxBC();

    void setForceNonBrown(const std::vector<double> &forceNonBrown);
    void setVelocityNonBrown(const std::vector<double> &velNonBrown);
    void calcVelocityBrown(); // write back to sylinder.velBrown
    void calcVelocityKnown();
    Teuchos::RCP<const TV> getForceKnown() const { return forceKnownRcp.getConst(); }
    Teuchos::RCP<const TV> getVelocityKnown() const { return velocityKnownRcp.getConst(); };
    Teuchos::RCP<const TV> getVelocityBrown() const { return velocityBrownRcp.getConst(); };
    Teuchos::RCP<const TV> getVelocityNonBrown() const { return velocityKnownRcp.getConst(); };
    Teuchos::RCP<const TV> getForceCol() const { return forceColRcp.getConst(); };
    Teuchos::RCP<const TV> getVelocityCol() const { return velocityColRcp.getConst(); };

    void collectWallCollisionBlocks(); // collect wall collision constraints
    void collectPairCollisionBlocks(); // collect pair collision constraints
    void resolveCollision();           // resolve collision
    void saveVelocityCollision();      // write back to sylinder.velCol

    void calcMobMatrix();
    void calcMobOperator();
    Teuchos::RCP<TCMAT> getMobMatrix() { return mobilityMatrixRcp; };
    Teuchos::RCP<TOP> getMobOperator() { return mobilityOperatorRcp; };

    void moveEuler(); // Euler step update position and orientation, with given velocity

    // statistics
    void calcColStress(); // collision stress
    void calcVolFrac();   // volume fraction

    // add new particles and assign new gid
    void addNewSylinder(std::vector<Sylinder> &newCylinder);

  private:
    // timestep Count
    int snapID;
    int stepCount;

    // FDPS stuff
    PS::DomainInfo dinfo;
    PS::ParticleSystem<Sylinder> sylinderContainer; // sylinders
    std::unique_ptr<TreeSylinderNear> treeSylinderNearPtr;
    int treeSylinderNumber;

    // Collision stuff
    std::shared_ptr<CollisionSolver> collisionSolverPtr;
    std::shared_ptr<CollisionCollector> collisionCollectorPtr;
    Teuchos::RCP<TV> forceKnownRcp;
    Teuchos::RCP<TV> velocityKnownRcp;
    Teuchos::RCP<TV> velocityBrownRcp;
    Teuchos::RCP<TV> velocityNonBrownRcp;
    Teuchos::RCP<TV> forceColRcp;
    Teuchos::RCP<TV> velocityColRcp;

    // MPI stuff
    std::shared_ptr<TRngPool> rngPoolPtr;      // thread safe rng
    Teuchos::RCP<const TCOMM> commRcp;         // mpi communicator
    Teuchos::RCP<TMAP> sylinderMapRcp;         // 1 dof per sphere
    Teuchos::RCP<TMAP> sylinderMobilityMapRcp; // 6 dof per sphere
    Teuchos::RCP<TCMAT> mobilityMatrixRcp;     // block-diagonal mobility matrix
    Teuchos::RCP<TOP> mobilityOperatorRcp;     // full mobility operator (matrix-free)

    // initialize
    void setInitialFromConfig();
    void setInitialFromFile(const std::string &filename);

    void updateSylinderMap(); // update sylinderMap and sylinderMobilityMap

    void setDomainInfo();

    void setPosWithWall(); // directly set the position of sylinders according to wall

    std::pair<int, int> getMaxGid();

    void genOrient(Equatn &orient, const double px, const double py, const double pz, const int threadId = 0);
};

#endif