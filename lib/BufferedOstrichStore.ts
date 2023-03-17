import type * as RDF from '@rdfjs/types';

export class VMIterator implements Iterator<RDF.Quad> {
  private index: number;
  private buffer: RDF.Quad[];
  private done: boolean;
  public constructor(public readonly bufferSize: number) {
    this.index = 0;
    this.buffer = [];
    this.done = false;
  }

  public next(...args: [] | [undefined]): IteratorResult<RDF.Quad> {
    throw new Error('Method not implemented.');
  }

  public return?(value?: any): IteratorResult<RDF.Quad> {
    throw new Error('Method not implemented.');
  }

  public throw?(error?: any): IteratorResult<RDF.Quad> {
    throw new Error('Method not implemented.');
  }
}

export class BufferedOstrichStore {
  public constructor(
    public readonly dataFactory: RDF.DataFactory,
    public readonly readOnly: boolean,
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

  public searchTriplesVersionMaterialized(
    subject: RDF.Term | undefined | null,
    predicate: RDF.Term | undefined | null,
    object: RDF.Term | undefined | null,
    options?: { offset?: number; limit?: number; version?: number },
  ): Promise<{ triples: Iterator<RDF.Quad>; cardinality: number; exactCardinality: boolean }> {

  }
}
