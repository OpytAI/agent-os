import type { MapHostCall } from "@mc/host";
import { tierCaps } from "@mc/contracts/constants";
import type { CreateOptions } from "../types.js";
import { CompilerEngine } from "./engine.js";
import { hexBytes, incHostCall } from "./metrics.js";
import { handleGuestRequest } from "./request.js";

/** Unset boot tier is the full capability mask. An unknown tier is an empty mask. */
export function callerMaskForBootTier(tier: number): number {
  if (tier === 0) return 0xff;
  return tierCaps(tier) & 0xff;
}

/** Tier stamped when the guest omits one. Unset boots as full. */
export function defaultTierForBootTier(tier: number): number {
  if (tier >= 1 && tier <= 4) return tier;
  return 1;
}

export async function bootstrapHostCompiler(
  tools: MapHostCall,
  opts: CreateOptions,
  env: {
    callerMask: number;
    defaultTier: number;
    memCeiling: number;
    fuelCeiling: bigint;
    tableCeiling: number;
  },
): Promise<CompilerEngine | null> {
  const compiler = opts.compiler;
  if (compiler == null) return null;
  const explicit = compiler === true ? {} : compiler;
  const engine = await CompilerEngine.load({
    engine: explicit.engine,
    pack: explicit.pack,
    profile: explicit.profile,
  });
  tools.registerRaw("compiler", async (body, _context) => {
    try {
      const frame = await handleGuestRequest({
        body,
        profileDigest: engine.profileDigest,
        packDigest: engine.packDigest,
        callerMask: env.callerMask,
        defaultTier: env.defaultTier,
        memCeiling: env.memCeiling,
        fuelCeiling: env.fuelCeiling,
        tableCeiling: env.tableCeiling,
        compile: (sourcePackage) => engine.compile(sourcePackage),
      });
      incHostCall("ok");
      return frame;
    } catch (error) {
      incHostCall("rejected");
      throw error;
    }
  });
  console.info(
    `luauc compiler_attached profile_sha256=${hexBytes(engine.profileDigest)} pack_sha256=${hexBytes(engine.packDigest)}`,
  );
  return engine;
}
