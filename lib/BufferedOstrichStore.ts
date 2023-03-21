import * as fs from 'fs';
import type * as RDF from '@rdfjs/types';
import { stringQuadToQuad, termToString, quadToStringQuad } from 'rdf-string';
import type { IBufferedOstrichStoreNative, IQueryProcessor } from './IBufferedOstrichStoreNative';
import type { IQuadDelta } from './utils';
import { serializeTerm, strcmp } from './utils';
const ostrichNative = require('../build/Release/ostrich.node');

/**
 * An Iterator to iterate over query results.
 * Maintain an internal buffer of triples.
 */
export class QueryIterator implements Iterator<RDF.Quad> {
  protected index: number;
  protected buffer: RDF.Quad[];
  protected done: boolean;
  protected count: number;

  public constructor(
    public readonly bufferSize: number,
    protected readonly queryProcessor: IQueryProcessor,
    protected readonly finishCallback: (() => void),
  ) {
    this.index = 0;
    this.buffer = [];
    this.done = false;
    this.count = 0;
  }

  public next(): IteratorResult<RDF.Quad> {
    // Native is done, and no more triples in the buffer
    if (this.done && this.index === this.buffer.length) {
      this.finishCallback();
      return {
        done: true,
        value: undefined,
      };
    }
    // We reached the end of the buffer or the buffer is empty -> we get more triples from native
    if (!this.done && (this.buffer.length === this.index || this.buffer.length === 0)) {
      this.buffer = [];
      this.queryProcessor._next(this.bufferSize, (error, triples, done) => {
        triples.forEach(triple => this.buffer.push(stringQuadToQuad(triple)));
        this.done = done;
      });
      this.index = 0;
    }
    // There is at least one triple in the buffer, we return it and increment the index
    const triple = this.buffer[this.index];
    this.index++;
    this.count++;
    return {
      done: false,
      value: triple,
    };
  }
}

export class BufferedOstrichStore {
  private operations = 0;
  private readonly _operationsCallbacks: (() => void)[] = [];
  private _isClosingCallbacks?: ((error: Error) => void)[];

  public constructor(
    public readonly native: IBufferedOstrichStoreNative,
    public readonly bufferSize: number,
    public readonly readOnly: boolean,
    public readonly features: Record<string, boolean>,
  ) {}

  /**
   * The number of available versions.
   */
  public get maxVersion(): number {
    throw new Error('Method not implemented.');
  }

  /**
   * If the store was closed.
   */
  public get closed(): boolean {
    throw new Error('Method not implemented.');
  }

  /**
   * Searches the document for triples with the given subject, predicate, object and version
   * for a version materialized query.
   * @param subject An RDF term.
   * @param predicate An RDF term.
   * @param object An RDF term.
   * @param options Options
   */
  public searchTriplesVersionMaterialized(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
    options?: { offset?: number; version?: number },
  ): Promise<Iterator<RDF.Quad>> {
    return new Promise((resolve, reject) => {
      if (this.closed) {
        return reject(new Error('Attempted to query a closed OSTRICH store'));
      }
      if (this.maxVersion < 0) {
        return reject(new Error('Attempted to query an OSTRICH store without versions'));
      }
      const offset = options && options.offset ? Math.max(0, options.offset) : 0;
      const version = options && (options.version || options.version === 0) ? options.version : -1;
      this.operations++;
      this.native._searchTriplesVersionMaterialized(
        serializeTerm(subject),
        serializeTerm(predicate),
        serializeTerm(object),
        offset,
        version,
        (error, queryProcessor) => {
          if (error) {
            return reject(error);
          }
          resolve(
            new QueryIterator(this.bufferSize, queryProcessor, () => this.finishOperation()),
          );
        },
      );
    });
  }

  /**
   * Gives an approximate number of matches of triples with the given subject, predicate, object and version
   * for a version materialized query.
   * @param subject An RDF term.
   * @param predicate An RDF term.
   * @param object An RDF term.
   * @param version The version to obtain.
   */
  public async countTriplesVersionMaterialized(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
    version = -1,
  ): Promise<{ cardinality: number; exactCardinality: boolean }> {
    return new Promise((resolve, reject) => {
      this.operations++;
      this.native._countTriplesVersionMaterialized(
        serializeTerm(subject),
        serializeTerm(predicate),
        serializeTerm(object),
        version,
        (error, totalCount, hasExactCount) => {
          this.finishOperation();
          if (error) {
            return reject(error);
          }
          resolve({
            cardinality: totalCount,
            exactCardinality: hasExactCount,
          });
        },
      );
    });
  }

  /**
   * Searches the document for triples with the given subject, predicate, object, versionStart and versionEnd
   * for a delta materialized query.
   * @param subject An RDF term.
   * @param predicate An RDF term.
   * @param object An RDF term.
   * @param options Options
   */
  public searchTriplesDeltaMaterialized(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
    options: { offset?: number; limit?: number; versionStart: number; versionEnd: number },
  ): Promise<Iterator<RDF.Quad>> {
    return new Promise((resolve, reject) => {
      if (this.closed) {
        return reject(new Error('Attempted to query a closed OSTRICH store'));
      }
      if (this.maxVersion < 0) {
        return reject(new Error('Attempted to query an OSTRICH store without versions'));
      }
      const offset = options && options.offset ? Math.max(0, options.offset) : 0;
      const versionStart = options.versionStart;
      const versionEnd = options.versionEnd;
      if (versionStart >= versionEnd) {
        return reject(new Error(`'versionStart' must be strictly smaller than 'versionEnd'`));
      }
      if (versionEnd > this.maxVersion) {
        return reject(new Error(`'versionEnd' can not be larger than the maximum version (${this.maxVersion})`));
      }
      this.operations++;
      this.native._searchTriplesDeltaMaterialized(
        serializeTerm(subject),
        serializeTerm(predicate),
        serializeTerm(object),
        offset,
        versionStart,
        versionEnd,
        (error, queryProcessor) => {
          if (error) {
            return reject(error);
          }
          resolve(
            new QueryIterator(this.bufferSize, queryProcessor, () => this.finishOperation()),
          );
        },
      );
    });
  }

  /**
   * Gives an approximate number of matches of triples with the given subject, predicate, object,
   * versionStart and versionEnd for a delta materialized query.
   * @param subject An RDF term.
   * @param predicate An RDF term.
   * @param object An RDF term.
   * @param versionStart The initial version.
   * @param versionEnd The final version.
   */
  public async countTriplesDeltaMaterialized(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
    versionStart: number,
    versionEnd: number,
  ): Promise<{ cardinality: number; exactCardinality: boolean }> {
    return new Promise((resolve, reject) => {
      this.operations++;
      this.native._countTriplesDeltaMaterialized(
        serializeTerm(subject),
        serializeTerm(predicate),
        serializeTerm(object),
        versionStart,
        versionEnd,
        (error, totalCount, hasExactCount) => {
          this.finishOperation();
          if (error) {
            return reject(error);
          }
          resolve({
            cardinality: totalCount,
            exactCardinality: hasExactCount,
          });
        },
      );
    });
  }

  /**
   * Searches the document for triples with the given subject, predicate and object for a version query.
   * @param subject An RDF term.
   * @param predicate An RDF term.
   * @param object An RDF term.
   * @param options Options
   */
  public searchTriplesVersion(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
    options?: { offset?: number; limit?: number },
  ): Promise<Iterator<RDF.Quad>> {
    return new Promise((resolve, reject) => {
      if (this.closed) {
        return reject(new Error('Attempted to query a closed OSTRICH store'));
      }
      if (this.maxVersion < 0) {
        return reject(new Error('Attempted to query an OSTRICH store without versions'));
      }
      const offset = options && options.offset ? Math.max(0, options.offset) : 0;

      this.operations++;
      this.native._searchTriplesVersion(
        serializeTerm(subject),
        serializeTerm(predicate),
        serializeTerm(object),
        offset,
        (error, queryProcessor) => {
          if (error) {
            return reject(error);
          }
          resolve(
            new QueryIterator(this.bufferSize, queryProcessor, () => this.finishOperation()),
          );
        },
      );
    });
  }

  /**
   * Gives an approximate number of matches of triples with the given subject, predicate and object for a version query.
   * @param subject An RDF term.
   * @param predicate An RDF term.
   * @param object An RDF term.
   */
  public async countTriplesVersion(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
  ): Promise<{ cardinality: number; exactCardinality: boolean }> {
    return new Promise((resolve, reject) => {
      this.operations++;
      this.native._countTriplesVersion(
        serializeTerm(subject),
        serializeTerm(predicate),
        serializeTerm(object),
        (error, totalCount, hasExactCount) => {
          this.finishOperation();
          if (error) {
            return reject(error);
          }
          resolve({
            cardinality: totalCount,
            exactCardinality: hasExactCount,
          });
        },
      );
    });
  }

  /**
   * Appends the given triples.
   * @param triples The triples to append, annotated with addition: true or false as the given version.
   * @param version The version to append at, defaults to the last version
   */
  public append(triples: IQuadDelta[], version = -1): Promise<number> {
    // Make sure our triples are sorted
    triples = triples.sort((left, right) => {
      const compS = strcmp(termToString(left.subject), termToString(right.subject));
      if (compS === 0) {
        const compP = strcmp(termToString(left.predicate), termToString(right.predicate));
        if (compP === 0) {
          return strcmp(termToString(left.object), termToString(right.object));
        }
        return compP;
      }
      return compS;
    });

    return this.appendSorted(triples, version);
  }

  /**
   * Appends the given triples.
   * The array is assumed to be sorted in SPO-order.
   * @param triples The triples to append, annotated with addition: true or false as the given version.
   * @param version The version to append at, defaults to the last version
   */
  public appendSorted(triples: IQuadDelta[], version = -1): Promise<number> {
    return new Promise((resolve, reject) => {
      if (this.closed) {
        return reject(new Error('Attempted to append to a closed OSTRICH store'));
      }
      if (this.readOnly) {
        return reject(new Error('Attempted to append to an OSTRICH store in read-only mode'));
      }

      this.operations++;
      if (version === -1) {
        version = this.maxVersion + 1;
      }
      this.native._append(
        version,
        triples.map(triple => ({ addition: triple.addition, ...quadToStringQuad(triple) })),
        (error, insertedCount) => {
          this.finishOperation();
          if (error) {
            return reject(error);
          }
          resolve(insertedCount);
        },
      );
    });
  }

  public finishOperation(): void {
    this.operations = this.operations === 0 ? 0 : this.operations--;
    // Call the operations-callbacks if no operations are going on anymore.
    if (!this.operations) {
      for (const cb of this._operationsCallbacks) {
        cb();
      }
    }
  }

  public close(remove = false): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      this._closeInternal(remove, (error?: Error) => {
        if (error) {
          return reject(error);
        }
        resolve();
      });
    });
  }

  protected _closeInternal(remove: boolean, callback: (error?: Error) => void): void {
    // eslint-disable-next-line @typescript-eslint/no-this-alias,consistent-this
    const self: BufferedOstrichStore = this;
    if (this._isClosingCallbacks) {
      this._isClosingCallbacks.push(callbackSafe);
      return;
    }
    this._isClosingCallbacks = [ callbackSafe ];
    // If no appends are being done, close immediately,
    // otherwise wait for appends to finish.
    if (!this.operations) {
      this.native._close(remove, onClosed);
    } else {
      this._operationsCallbacks.push(() => {
        self.native._close(remove, onClosed);
      });
    }

    function onClosed(error?: Error): void {
      self._isClosingCallbacks!.forEach((cb: (error: Error) => void) => cb(error!));
      delete self._isClosingCallbacks;
    }

    function callbackSafe(error: Error): void {
      if (callback) {
        return callback(error);
      }
    }
  }
}

/**
 * Creates a buffered Ostrich store for the given path.
 * @param path Path to an OSTRICH store.
 * @param bufferSize The number of triples to buffer during querying
 * @param options Options for opening the store.
 */
export function fromPathBuffered(
  path: string,
  bufferSize: number,
  options?: {
    readOnly?: boolean;
    strategyName?: string;
    strategyParameter?: string;
    dataFactory?: RDF.DataFactory;
  },
): Promise<BufferedOstrichStore> {
  return new Promise((resolve, reject) => {
    if (path.length === 0) {
      reject(new Error(`Invalid path: ${path}`));
    }
    if (!path.endsWith('/')) {
      path += '/';
    }

    if (!options) {
      options = {};
    }

    if (typeof options.strategyName !== 'string') {
      options.strategyName = 'never';
      options.strategyParameter = '0';
    }

    if (bufferSize < -1) {
      bufferSize = -1;
    }

    // eslint-disable-next-line no-sync
    if (!options.readOnly && !fs.existsSync(path)) {
      try {
        // eslint-disable-next-line no-sync
        fs.mkdirSync(path);
      } catch (error: unknown) {
        throw new Error(`Unable to create new OSTRICH store at '${path}': ${(<Error> error).message}`);
      }
    }

    // Construct the native OstrichStore
    ostrichNative.createBufferedOstrichStore(
      path,
      options.readOnly,
      options.strategyName,
      options.strategyParameter,
      (error: Error, native: IBufferedOstrichStoreNative) => {
        // Abort the creation if any error occurred
        if (error) {
          return reject(error);
        }
        const document = new BufferedOstrichStore(
          native,
          bufferSize,
          Boolean(options!.readOnly),
          Object.freeze({
            searchTriplesVersionMaterialized: true,
            countTriplesVersionMaterialized: true,
            searchTriplesDeltaMaterialized: true,
            countTriplesDeltaMaterialized: true,
            searchTriplesVersion: true,
            countTriplesVersion: true,
            appendVersionedTriples: !options!.readOnly,
          }),
        );
        resolve(document);
      },
    );
  });
}

