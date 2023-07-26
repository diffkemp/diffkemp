// Arrows for navigating to next/previous difference, ...
// Author: Lukas Petr

import { PropTypes } from 'prop-types';

import ButtonGroup from 'react-bootstrap/ButtonGroup';
import Button from 'react-bootstrap/Button';

/**
 * Arrows for navigating to next/previous difference, ...
 * @param {Object} props
 */
export default function NavigationArrows({
  onPrevClick,
  onNextClick,
  nextText,
  prevText,
  disablePrev,
  disableNext,
  className,
}) {
  return (
    <ButtonGroup className={className}>
      <Button
        onClick={onPrevClick}
        variant="outline-primary"
        size="sm"
        disabled={disablePrev}
      >
        {`< Prev ${prevText}`}
      </Button>
      <Button
        onClick={onNextClick}
        variant="outline-primary"
        size="sm"
        disabled={disableNext}
      >
        {`Next ${nextText} >`}
      </Button>
    </ButtonGroup>
  );
}

NavigationArrows.propTypes = {
  onPrevClick: PropTypes.func.isRequired,
  onNextClick: PropTypes.func.isRequired,
  nextText: PropTypes.string.isRequired,
  prevText: PropTypes.string.isRequired,
  disablePrev: PropTypes.bool.isRequired,
  disableNext: PropTypes.bool.isRequired,
  className: PropTypes.string,
};
