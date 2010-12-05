#include "time_normalizer.h"
#include "eternity_time_normalizer.h"
#include "epoch_time_normalizer.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

TimeNormalizer*
TimeNormalizer::create(SInt32 num_entities)
{
   string type = Sim()->getCfg()->getString("time_normalizer/type", "eternity");
   if (type == "eternity")
      return new EternityTimeNormalizer(num_entities);
   else if (type == "epoch")
      return new EpochTimeNormalizer(num_entities);
   else
      LOG_PRINT_ERROR("Unrecognized Time Normalizer Type(%s)", type.c_str());
   return (TimeNormalizer*) NULL;
}
