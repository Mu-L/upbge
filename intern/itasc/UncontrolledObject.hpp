/* SPDX-FileCopyrightText: 2009 Ruben Smits
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */

/** \file
 * \ingroup intern_itasc
 */

#ifndef UNCONTROLLEDOBJECT_HPP_
#define UNCONTROLLEDOBJECT_HPP_

#include "eigen_types.hpp"

#include "Object.hpp"
namespace iTaSC{

class UncontrolledObject: public Object {
protected:
	unsigned int m_nu, m_nf;
	e_vector m_xudot;
	std::vector<e_matrix> m_JuArray;

public:
    UncontrolledObject();
    virtual ~UncontrolledObject();

	virtual void initialize(unsigned int _nu, unsigned int _nf);
	virtual const e_matrix& getJu(unsigned int frameIndex) const;
    virtual const e_vector& getXudot() const {return m_xudot;}
	virtual void updateCoordinates(const Timestamp& timestamp)=0;
    virtual unsigned int getNrOfCoordinates(){return m_nu;};
    virtual unsigned int getNrOfFrames(){return m_nf;};

};

}

#endif /* UNCONTROLLEDOBJECT_H_ */
