import FWCore.ParameterSet.Config as cms
from Validation.CSCRecHits.cscRecHitPSet import *

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer
cscRecHitValidation = DQMEDAnalyzer(
    'CSCRecHitValidation',
    cscRecHitPSet,
    doSim = cms.bool(True),
    simHitsTag = cms.InputTag("mix","g4SimHitsMuonCSCHits")
)

from Configuration.Eras.Modifier_fastSim_cff import fastSim
fastSim.toModify(cscRecHitValidation, simHitsTag = "mix:MuonSimHitsMuonCSCHits")
