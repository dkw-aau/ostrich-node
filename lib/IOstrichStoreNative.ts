import type { IStringQuad } from 'rdf-string';
import type { IStringQuadDelta, IStringQuadVersion } from './utils';

/**
 * A native OSTRICH store that corresponds to the implementation in OstrichStore.cc
 */
export interface IOstrichStoreNative {
  maxVersion: number;
  closed: boolean;
  _close: (remove: boolean, callback: (error?: Error) => void) => void;
  _searchTriplesVersionMaterialized: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    offset: number,
    limit: number,
    version: number,
    cb: (error: Error | undefined, triples: IStringQuad[], totalCount: number, hasExactCount: boolean) => void,
  ) => void;
  _countTriplesVersionMaterialized: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    version: number,
    cb: (error: Error | undefined, totalCount: number, hasExactCount: boolean) => void,
  ) => void;
  _searchTriplesDeltaMaterialized: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    offset: number,
    limit: number,
    versionStart: number,
    versionEnd: number,
    cb: (error: Error | undefined, triples: IStringQuadDelta[], totalCount: number, hasExactCount: boolean) => void,
  ) => void;
  _countTriplesDeltaMaterialized: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    versionStart: number,
    versionEnd: number,
    cb: (error: Error | undefined, totalCount: number, hasExactCount: boolean) => void,
  ) => void;
  _searchTriplesVersion: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    offset: number,
    limit: number,
    cb: (error: Error | undefined, triples: IStringQuadVersion[], totalCount: number, hasExactCount: boolean) => void,
  ) => void;
  _countTriplesVersion: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    cb: (error: Error | undefined, totalCount: number, hasExactCount: boolean) => void,
  ) => void;
  _append: (
    version: number,
    triples: IStringQuadDelta[],
    cb: (error: Error | undefined, insertedCount: number) => void,
  ) => void;
}
