import type { IStringQuad } from 'rdf-string';
import type { IStringQuadDelta, IStringQuadVersion } from './utils';

export interface IQueryProcessor {
  _next: (
    number: number,
    callback: (error: Error | undefined, triples: IStringQuad[]) => void,
  ) => void;
}

export interface IVersionMaterializationProcessor extends IQueryProcessor {
  _next: (
    number: number,
    callback: (error: Error | undefined, triples: IStringQuad[]) => void,
  ) => void;
}

export interface IDeltaMaterializationProcessor extends IQueryProcessor {
  _next: (
    number: number,
    callback: (error: Error | undefined, triples: IStringQuadDelta[]) => void,
  ) => void;
}

export interface IVersionQueryProcessor extends IQueryProcessor {
  _next: (
    number: number,
    callback: (error: Error | undefined, triples: IStringQuadVersion[]) => void,
  ) => void;
}

/**
 * A native OSTRICH store that corresponds to the implementation in BufferedOstrichStore.cc
 */
export interface IBufferedOstrichStoreNative {
  maxVersion: number;
  closed: boolean;
  _close: (remove: boolean, callback: (error?: Error) => void) => void;
  _searchTriplesVersionMaterialized: (
    subject: string | null,
    predicate: string | null,
    object: string | null,
    offset: number,
    version: number,
  ) => IVersionMaterializationProcessor;
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
    versionStart: number,
    versionEnd: number,
  ) => IDeltaMaterializationProcessor;
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
  ) => IVersionQueryProcessor;
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
