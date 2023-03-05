// Representation of found differences of semantic comparision
// provided in the form of YAML.
// Author: Lukas Petr

function compareFunctionNames(nameA, nameB) {
  return nameA.toLowerCase().localeCompare(nameB.toLowerCase());
}
/**
 * Result of diffkemp compare obtained from YAML file.
 */
export default class Result {
  /**
   * Map
   * - keys: names of compared functions
   * - values: Map
   *   - keys: names of differing functions for the compared function
   *   - values: diff (part of YAML) for the compared and differing function
   * Note: The names of functions are inserted in sorted order.
   */
  #allCompFuns;

  /**
   * Map
   * - keys: names of differing functions
   * - values: sorted array of names of compared functions which differs in the differing function
   * Note: The names of functions are inserted in sorted order.
   */
  #allDiffFuns;

  /** Information about definitions of functions. */
  definitions;

  /** Absolute path to old snapshot. */
  oldSnapshot;

  /** Absolute path to new snapshot. */
  newSnapshot;

  /**
   * Create Result from YAML file.
   * @param {Object} yaml YAML file describing result of diffkemp compare.
   */
  constructor(yaml) {
    this.definitions = yaml.definitions;
    this.oldSnapshot = yaml['old-snapshot'];
    this.newSnapshot = yaml['new-snapshot'];
    // Sort results by compared function name
    yaml.results.sort((resultA, resultB) => (
      compareFunctionNames(resultA.function, resultB.function)
    ));
    // Sort differing functions of each compared function
    yaml.results.forEach((result) => {
      result.diffs.sort((diffA, diffB) => compareFunctionNames(diffA.function, diffB.function));
    });
    this.#createAllCompAndDiffFuns(yaml.results);
  }

  /**
   * Creates allCompFuns and allDiffFuns fields.
   * Expects that names of compared and differing functions are sorted in the `results`.
   * @param results - Differences found by DiffKemp compare phase.
   */
  #createAllCompAndDiffFuns(results) {
    // Map(compFunName, Map(diffFunName, diff))
    this.#allCompFuns = new Map();
    // Map(diffFunName, [...compFunNames])
    this.#allDiffFuns = new Map();
    // foreach result (compared function)
    results.forEach((result) => {
      // Map(diffFunName, diff)
      const diffFunsForComp = new Map();
      // for each diff (differing function)
      result.diffs.forEach((diff) => {
        diffFunsForComp.set(diff.function, diff);
        if (this.#allDiffFuns.has(diff.function)) {
          this.#allDiffFuns.get(diff.function).push(result.function);
        } else {
          this.#allDiffFuns.set(diff.function, [result.function]);
        }
      });
      this.#allCompFuns.set(result.function, diffFunsForComp);
    });
    // sort keys (differing function names) in the map
    this.#allDiffFuns = new Map([...this.#allDiffFuns]
      .sort((a, b) => compareFunctionNames(a[0], b[0])));
  }

  /**
   * Returns sorted array of all compared functions in which was found difference.
   * @param {String} [diffFunName=null] If specified returns only compared
   * functions, which have difference in this function.
   */
  getCompFuns(diffFunName = null) {
    if (diffFunName !== null) {
      return this.#allDiffFuns.get(diffFunName);
    }
    return [...this.#allCompFuns.keys()];
  }

  /**
   * Returns sorted array of all differing functions.
   * @param {String} [compFunName=null] If specified returns only differing
   * functions for this compared function.
   */
  getDiffFuns(compFunName = null) {
    if (compFunName !== null) {
      return [...this.#allCompFuns.get(compFunName).keys()];
    }
    return [...this.#allDiffFuns.keys()];
  }

  /**
   * Returns object which contains old callstack and new callstack for
   * specified compared and differing function.
   */
  getDiff(compFunName, diffFunName) {
    return this.#allCompFuns.get(compFunName).get(diffFunName);
  }

  /**
   * Returns true if compared function is first in ordered array of compared
   * functions.
   */
  isFirstCompFun(compFunName) {
    return this.getCompFuns()[0] === compFunName;
  }

  /**
   * Returns true if compared function is last in ordered array of compared
   * functions.
   */
  isLastCompFun(compFunName) {
    const comparedFunctions = this.getCompFuns();
    return comparedFunctions[comparedFunctions.length - 1] === compFunName;
  }

  /**
   * Returns true if differing function is first in ordered array of differing
   * functions for compared function.
   */
  isFirstDiffFunForComp(compFunName, diffFunName) {
    const firstDiffFunName = this.getDiffFuns(compFunName)[0];
    return diffFunName === firstDiffFunName;
  }

  /**
   * Returns true if differing function is last in ordered array of differing
   * functions for compared function.
   */
  isLastDiffFunForComp(compFunName, diffFunName) {
    const diffFuns = this.getDiffFuns(compFunName);
    const lastDiffFunName = diffFuns[diffFuns.length - 1];
    return diffFunName === lastDiffFunName;
  }

  /**
   * Returns next compared function according specified compared function.
   * Return null if specified function was last.
   */
  getNextCompName(compFunName) {
    const compFunNames = this.getCompFuns();
    const index = compFunNames.indexOf(compFunName);
    if (index + 1 < compFunNames.length) {
      return compFunNames[index + 1];
    }
    return null;
  }

  /**
   * Returns next differing function for compared function
   * according specified compared and differing function.
   * Return null if specified function was last.
   */
  getNextDiffFunNameForComp(compFunName, diffFunName) {
    const diffFunNames = this.getDiffFuns(compFunName);
    const index = diffFunNames.indexOf(diffFunName);
    if (index + 1 < diffFunNames.length) {
      return diffFunNames[index + 1];
    }
    return null;
  }

  /**
   * Returns previous compared function according specified compared function.
   * Return null if specified function was first.
   */
  getPrevCompName(compFunName) {
    const compFunNames = this.getCompFuns();
    const index = compFunNames.indexOf(compFunName);
    if (index - 1 >= 0) {
      return compFunNames[index - 1];
    }
    return null;
  }

  /**
   * Returns previous differing function for compared function
   * according specified compared and differing function.
   * Return null if specified function was first.
   */
  getPrevDiffFunNameForComp(compFunName, diffFunName) {
    const diffFunNames = this.getDiffFuns(compFunName);
    const index = diffFunNames.indexOf(diffFunName);
    if (index - 1 >= 0) {
      return diffFunNames[index - 1];
    }
    return null;
  }

  /**
   * Returns first differing function for specified compared function.
   */
  getFirstDiffFunForComp(compFunName) {
    return this.getDiffFuns(compFunName)[0];
  }

  /**
   * Returns last differing function for specified compared function.
   */
  getLastDiffFunForComp(compFunName) {
    const diffFuns = this.getDiffFuns(compFunName);
    return diffFuns[diffFuns.length - 1];
  }
}
